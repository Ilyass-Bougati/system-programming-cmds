# Chat Hub — Application de chat centralisée

Application de discussion en architecture **client–serveur**. Un serveur unique,
le *Chat Hub*, relaie tous les messages : aucun client ne communique directement
avec un autre. Plusieurs utilisateurs peuvent discuter en temps réel, soit dans
le **salon général** (diffusion à tous), soit en **message privé** (acheminé
vers un seul destinataire).

## Organisation du code

Séparation claire serveur / client, avec un en-tête commun pour le protocole :

| Fichier      | Rôle                                                                    |
|--------------|-------------------------------------------------------------------------|
| `common.h`   | Protocole « fil », lecteur de lignes (`line_reader_t`), envoi fiable.    |
| `server.c`   | Le serveur `chat_serverd` : accept, threads clients, table, diffusion.   |
| `client.c`   | Le client `chat_client` : connexion, saisie clavier, affichage temps réel.|
| `Makefile`   | Compilation des deux binaires.                                           |

---

## 1. Dépendances et compilation

Aucune bibliothèque tierce : uniquement **`gcc`** et la **libc POSIX** (Linux).
La concurrence repose sur `pthread`.

```bash
make            # produit ./chat_serverd et ./chat_client
make clean      # supprime les binaires
```

Le `Makefile` compile avec `-Wall -Wextra -pthread` (compilation sans
avertissement).

---

## 2. Exécution (serveur puis clients)

Lancer **d'abord le serveur**, puis autant de clients que voulu.

```bash
# Terminal serveur
./chat_serverd --port 5555                  # port d'écoute
./chat_serverd --port 5555 --log chat.log   # avec historique horodaté
```

```bash
# Terminal(s) client(s)
./chat_client --server localhost --port 5555
```

| Programme       | Option              | Effet                                    | Défaut |
|-----------------|---------------------|------------------------------------------|--------|
| `chat_serverd`  | `--port <n>`        | Port d'écoute TCP                        | `5555` |
| `chat_serverd`  | `--log <fichier>`   | Journalise les évènements (optionnel)    | —      |
| `chat_client`   | `--server <hôte>`   | Adresse du serveur                       | —      |
| `chat_client`   | `--port <n>`        | Port du serveur                          | —      |

À la connexion, le client demande un **pseudo**. S'il est déjà pris ou invalide
(caractères autorisés : alphanumériques, `_`, `-`, max 31 caractères), le serveur
répond `ERR …` et l'utilisateur peut réessayer.

### Commandes côté client

| Commande                | Effet                                                |
|-------------------------|------------------------------------------------------|
| `<texte>`               | Diffuse `<texte>` à tous les autres clients          |
| `/msg <pseudo> <texte>` | Message privé acheminé vers `<pseudo>` uniquement    |
| `/users`                | Affiche la liste des participants connectés          |
| `/quit`                 | Déconnexion propre (notifiée aux autres)             |

---

## 3. Protocole de communication

Messages **texte**, **orientés ligne** (terminés par `\n`). Le premier mot est
le **verbe**. Format inspectable au `nc` et trivial à étendre.

### Client → Serveur

| Message              | Sémantique                                          |
|----------------------|-----------------------------------------------------|
| `NICK <nom>`         | Poignée de main. Doit être le premier message.      |
| `MSG <texte>`        | Diffusion à tous les autres clients.                |
| `PRIV <nom> <texte>` | Message privé vers `<nom>`.                         |
| `LIST`               | Demande la liste des participants.                  |
| `QUIT`               | Déconnexion explicite.                              |

### Serveur → Client

| Message              | Sémantique                                          |
|----------------------|-----------------------------------------------------|
| `OK`                 | Pseudo accepté (réponse à `NICK`).                  |
| `ERR <raison>`       | Pseudo refusé ou commande invalide.                 |
| `MSG <de> <texte>`   | Message public reçu d'un autre client.              |
| `PRIV <de> <texte>`  | Message privé reçu.                                 |
| `JOIN <nom>`         | Notification d'arrivée.                             |
| `LEAVE <nom>`        | Notification de départ (propre ou brutal).          |
| `LIST <n1> <n2> …`   | Réponse à `LIST`.                                   |
| `SYS <texte>`        | Message système générique.                          |

### Exemple de session (octets bruts)

```
C → S : NICK Kamal
S → C : OK
S → tous : JOIN Kamal
C → S : MSG Bonjour tout le monde !
S → autres : MSG Kamal Bonjour tout le monde !
C → S : PRIV Said Salut Said
S → Said : PRIV Kamal Salut Said
C → S : LIST
S → C : LIST Kamal Said
C → S : QUIT
S → tous : LEAVE Kamal
```

---

## 4. Schéma d'architecture

```
                        ┌─────────────────────────┐
                        │      Chat Hub (TCP)     │
   table clients[]      │                         │
   ┌───────────────┐    │  accept() ─── thread ───┼──── client 1 (Kamal)
   │ fd | pseudo|ts│◄───┤              ─────────  │
   │ fd | pseudo|ts│    │              ─ thread ──┼──── client 2 (Said)
   └───────────────┘    │              ─────────  │
        ▲  protégée     │              ─ thread ──┼──── client n …
        │  par mutex    └─────────────────────────┘
        └──── diffusion : copie des fd sous verrou, envoi hors verrou
```

* Le thread principal exécute `accept()` en boucle et crée un **`pthread`
  détaché par client** (`client_thread`).
* Une table `clients[MAX_CLIENTS]` (64), protégée par un `pthread_mutex_t`,
  maintient les connectés : `fd`, pseudo, horodatage de connexion.
* Côté serveur, chaque client suit un cycle de vie linéaire :
  `do_handshake` → `JOIN` → `serve_client` (boucle de commandes) → `LEAVE`.
* Côté client, deux flux concurrents : le **thread principal** lit le clavier,
  un **thread de réception** affiche les messages entrants.

---

## 5. Choix techniques majeurs

| Aspect            | Choix                              | Justification                                          |
|-------------------|------------------------------------|--------------------------------------------------------|
| Langage           | C (C11)                            | Cohérent avec les TP précédents `sockets1`, `sockets2` |
| Protocole         | TCP/IP                             | Ordre FIFO garanti, détection de déconnexion native    |
| Concurrence       | `pthread` (un thread par client)   | Modèle simple et lisible, suffisant pour ≤ 64 clients  |
| Format des messages| texte, lignes `\n`                | Inspectable au `nc`, facile à documenter et à étendre  |
| Interface client  | CLI (terminal)                     | Pas de dépendance graphique                            |
| Journal           | `--log <fichier>` (optionnel)      | Trace horodatée des évènements JOIN/MSG/PRIV/LEAVE     |

### Difficultés rencontrées

* **Affichage CLI concurrent.** Un message entrant peut arriver pendant que
  l'utilisateur saisit une commande. Avant chaque affichage, le client efface
  la ligne courante (`\r\033[K`) puis réaffiche l'invite `> `.
* **Fragmentation TCP.** TCP est un flux d'octets sans frontières de message :
  un `recv()` peut renvoyer une demi-ligne ou plusieurs lignes collées. Le
  `line_reader_t` (dans `common.h`) bufferise les octets et ne livre que des
  **lignes complètes** au code applicatif.
* **Client lent.** Un `send()` vers un client gelé pourrait bloquer la
  diffusion. La diffusion **copie les `fd` cibles sous verrou puis envoie hors
  verrou**, et un `SO_SNDTIMEO` de 5 s borne chaque envoi.
* **Robustesse aux déconnexions.** `MSG_NOSIGNAL` + `signal(SIGPIPE, SIG_IGN)`
  empêchent qu'un client disparu fasse mourir le serveur ; un `recv()` qui
  renvoie 0/-1 déclenche proprement la diffusion d'un `LEAVE`.

---

## Garanties attendues

* **Ordre FIFO** : assuré par TCP par socket ; chaque émission est faite par un
  seul thread.
* **Détection des déconnexions brutales** : `recv()` renvoie 0/-1 → libération
  du slot et notification `LEAVE` aux autres.
* **Absence d'interblocage / non-blocage du serveur** : la diffusion ne tient
  pas le verrou pendant les `send()`, et `SO_SNDTIMEO` empêche un client gelé de
  figer les autres. La boucle `accept()` reste toujours disponible.
* **Unicité du pseudo** : vérifiée sous verrou avant insertion ; doublon →
  `ERR name already taken`.

---

## Jeu de tests

### Scénario manuel

1. Démarrer le serveur : `./chat_serverd --port 5555 --log chat.log`
2. Ouvrir deux clients :
   * Term 1 : `./chat_client --server localhost --port 5555` → pseudo `Kamal`
   * Term 2 : `./chat_client --server localhost --port 5555` → pseudo `Said`
3. Term 2 tape `Salut !` → Term 1 affiche `Said: Salut !`
4. Term 2 tape `/msg Kamal Coucou en privé` → seul Term 1 voit
   `[private from Said] Coucou en privé`
5. Term 1 tape `/users` → affiche `[users] Kamal Said`
6. Term 2 tape `/quit` → Term 1 voit `[*] Said a quitté le chat`
7. Vérifier que `chat.log` contient les évènements horodatés.

### Vérifications rapides

* Pseudo en doublon → `[error] name already taken`
* Pseudo avec espaces / caractères interdits →
  `[error] invalid name (alnum, _-, max 31 chars)`
* Inspection au `nc` : `nc localhost 5555` puis taper `NICK Test` → `OK`.

---

## Extensions possibles

* **Sécurité** : TLS via OpenSSL, ou authentification par mot de passe.
* **Salons multiples** : remplacer la diffusion globale par un routage
  pseudo → canal.
* **UDP** : le protocole étant texte ligne à ligne, un mode datagramme
  (un datagramme = une ligne) serait simple à ajouter.
