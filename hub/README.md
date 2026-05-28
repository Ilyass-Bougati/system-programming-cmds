# Chat Hub — Application de Chat Centralisée

Serveur unique (Chat Hub) en architecture client–serveur. Tous les messages
échangés entre les clients transitent obligatoirement par le serveur.

## Choix techniques

| Aspect            | Choix                                | Justification                                          |
|-------------------|--------------------------------------|--------------------------------------------------------|
| Langage           | C (C11)                              | Cohérent avec les TP précédents `sockets1`, `sockets2` |
| Protocole         | TCP/IP                               | Ordre FIFO garanti, détection de déconnexion native    |
| Concurrence       | `pthread` (un thread par client)     | Modèle simple, lisible, suffisant pour ≤ 64 clients    |
| Format wire       | texte, lignes terminées par `\n`     | Inspectable au `nc`, facile à étendre                  |
| Interface client  | CLI (ligne de commande)              | Pas de dépendance graphique                            |
| Journal optionnel | `--log <fichier>` côté serveur       | Trace horodatée des évènements (JOIN/MSG/PRIV/LEAVE)   |

## Dépendances et compilation

Nécessite uniquement `gcc` et la libc POSIX (Linux). Aucune bibliothèque tierce.

```bash
make            # produit ./chat_serverd et ./chat_client
make clean
```

## Exécution

```bash
# Terminal serveur
./chat_serverd --port 5555
./chat_serverd --port 5555 --log chat.log   # avec historique

# Terminal client
./chat_client --server localhost --port 5555
```

À la connexion, le client demande un pseudo. Si le nom est déjà pris ou invalide
(caractères autorisés : alphanumériques, `_`, `-`, max 31 caractères), le
serveur répond `ERR …` et l’utilisateur peut réessayer.

## Commandes côté client

| Commande              | Effet                                                       |
|-----------------------|-------------------------------------------------------------|
| `<texte>`             | Diffuse `<texte>` à tous les autres clients                 |
| `/msg <pseudo> <txt>` | Message privé routé vers `<pseudo>` uniquement              |
| `/users`              | Demande la liste des participants connectés                 |
| `/quit`               | Déconnexion propre (notifiée aux autres)                    |

## Protocole de communication

Lignes ASCII terminées par `\n`. Le premier *token* est le verbe.

### Client → Serveur

| Message              | Sémantique                                              |
|----------------------|---------------------------------------------------------|
| `NICK <name>`        | Handshake. Doit être le premier message envoyé.         |
| `MSG <texte>`        | Diffusion à tous les autres clients.                    |
| `PRIV <name> <txt>`  | Message privé vers `<name>`.                            |
| `LIST`               | Demande la liste des participants.                      |
| `QUIT`               | Déconnexion explicite.                                  |

### Serveur → Client

| Message              | Sémantique                                              |
|----------------------|---------------------------------------------------------|
| `OK`                 | Pseudo accepté (réponse à `NICK`).                      |
| `ERR <raison>`       | Pseudo refusé ou commande invalide.                     |
| `MSG <from> <txt>`   | Diffusion reçue d’un autre client.                      |
| `PRIV <from> <txt>`  | Message privé reçu.                                     |
| `JOIN <name>`        | Notification d’arrivée.                                 |
| `LEAVE <name>`       | Notification de départ (propre ou brutale).             |
| `LIST <n1> <n2> …`   | Réponse à `LIST`.                                       |
| `SYS <texte>`        | Message système générique.                              |

Exemple de session brute :

```
C → S : NICK Kamal
S → C : OK
S → tous : JOIN Kamal
C → S : MSG Bonjour tout le monde !
S → autres : MSG Kamal Bonjour tout le monde !
C → S : PRIV Said Salut Kamal
S → Said : PRIV Kamal Salut Kamal
C → S : LIST
S → C : LIST Kamal Said
C → S : QUIT
S → tous : LEAVE Kamal
```

## Architecture interne

```
                       ┌────────────────────────┐
                       │     Chat Hub (TCP)     │
                       │                        │
                       │   accept() ── thread  ─┼──── client 1 (Kamal)
                       │              ────────  │
                       │              ─ thread ─┼──── client 2 (Said)
                       │              ────────  │
   table clients[] ◄───┼─ mutex                 ▼
   (fd, pseudo, ts)    └────────────────────────┘
```

* Le thread principal du serveur exécute `accept()` en boucle et crée un
  `pthread` détaché par client.
* Une table `clients[MAX_CLIENTS]` protégée par `pthread_mutex_t` maintient
  la liste des connectés (`fd`, pseudo, horodatage).
* Pour diffuser, on **copie** les `fd` cibles sous verrou puis on libère le
  verrou avant les `send()`. Cela évite qu’un client lent bloque toute la
  diffusion. Un `SO_SNDTIMEO` de 5 s borne chaque `send`.
* `MSG_NOSIGNAL` + `signal(SIGPIPE, SIG_IGN)` empêchent qu’un client
  disparu fasse mourir le serveur.

## Garanties

* **Ordre FIFO** : assuré par TCP par socket, et chaque émission est
  effectuée par un thread unique.
* **Déconnexion brutale** : le `recv()` du thread client retourne 0/-1,
  ce qui déclenche la diffusion d’un `LEAVE` et la libération du slot.
* **Non-blocage du serveur** : la diffusion ne tient pas le verrou pendant
  les `send()`, et `SO_SNDTIMEO` empêche un client gelé de bloquer les
  autres. La boucle `accept()` reste toujours disponible.
* **Unicité du pseudo** : vérifiée sous verrou avant insertion dans la
  table ; doublon → `ERR name already taken`.

## Difficultés rencontrées

* **Affichage CLI concurrent.** Les messages entrants doivent s’insérer
  proprement même quand l’utilisateur est en train de saisir une commande.
  La séquence `\r\033[K` (retour ligne + effacement) est imprimée avant
  chaque message reçu, puis le prompt `> ` est réaffiché.
* **Fragmentation TCP.** Le code n’assume jamais qu’un `recv()` renvoie une
  ligne complète : un `line_reader_t` (dans `common.h`) bufferise les
  octets et ne livre que des lignes complètes au code applicatif.
* **Slow client.** Les `send()` de diffusion sont effectués hors verrou et
  avec un timeout pour éviter qu’un client gelé fige le serveur.

## Jeu de tests

Scénario manuel (cf. capture d’écran attendue dans le sujet) :

1. Lancer le serveur : `./chat_serverd --port 5555 --log chat.log`
2. Ouvrir deux terminaux clients :
   * Term 1 : `./chat_client --server localhost --port 5555` → pseudo `Kamal`
   * Term 2 : `./chat_client --server localhost --port 5555` → pseudo `Said`
3. Dans Term 2, taper `Salut !` → Term 1 voit `Said: Salut !`
4. Dans Term 2, taper `/msg Kamal Coucou en privé` → seul Term 1 voit
   `[private from Said] Coucou en privé`.
5. Dans Term 1, taper `/users` → affiche `[users] Kamal Said`.
6. Dans Term 2, taper `/quit` → Term 1 voit `[*] Said a quitté le chat`.
7. Vérifier que `chat.log` contient les évènements horodatés.

Tests automatiques rapides (voir aussi commande `bash` ci-dessous) :

* Pseudo en doublon → `[error] name already taken`
* Pseudo avec espaces → `[error] invalid name (alnum, _-, max 31 chars)`

## Extensions possibles

* **Sécurité** : ajouter TLS via OpenSSL, ou un mot de passe par pseudo.
* **UDP** : le protocole étant texte ligne-à-ligne, un mode UDP est
  trivial à ajouter (un datagramme = une ligne).
* **Salons multiples** : remplacer la diffusion globale par un mapping
  pseudo → canal et router `MSG` au canal courant.
