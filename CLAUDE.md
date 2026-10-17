# ft_malcolm

> Ce fichier hérite des règles générales de `../CLAUDE.md` (contexte 42, pédagogie,
> pas de code/correction tant que non explicitement demandé, débogage guidé,
> prépa soutenance par questions). Les points ci-dessous sont **spécifiques au projet**.

## Mode de travail (spécifique à ce projet)

> Ce mode **complète et précise** la règle générale « pas de code tant que non
> explicitement demandé » de `../CLAUDE.md`. Ici, l'étudiant m'autorise à coder
> **sur demande**, selon le cadre suivant :

- **L'étudiant pilote** : il organise le projet, son architecture et son fonctionnement.
  Moi je suis un **agent de code** qui implémente **exactement** ce qu'il demande.
- **Ne jamais aller plus loin que ce qui est demandé.** Pas de fichier, de fonction,
  de refacto, d'« amélioration » ou d'anticipation non sollicitée.
- **Exception = signalement, pas initiative** : si quelque chose me paraît nécessaire
  d'un point de vue **sécurité** ou **fonctionnement/correction** du programme, je ne
  le fais pas d'office — je **le dis et le suggère avant**, en l'expliquant, pour qu'il
  comprenne et décide. C'est lui qui tranche.
- Je garde en parallèle la **posture pédagogique** : expliquer la théorie, poser des
  questions, l'aider à déboguer par lui-même, préparer la soutenance.

## De quoi il s'agit

Premier projet de la branche sécurité réseau de 42. Implémentation de l'**ARP
spoofing/poisoning** (Man In The Middle de base) en C, exploitant le fonctionnement
du protocole ARP (couche 2 / liaison du modèle OSI).

Références à lire : **RFC 826** (ARP) et **RFC 7042**.

## Cadre légal / éthique (important)

- Projet **pédagogique et encadré** par 42. Contexte autorisé.
- On ne spoofe **que des IP qui nous appartiennent** (notre propre VM). Le sujet est
  explicite : spoofer d'autres IP = problèmes et/ou sanctions.
- Tout le travail se fait **dans une VM** (kernel Linux > 3.14, grading conçu sur
  Debian 7.0 stable).

## Spécification mandatory

- Exécutable nommé **`ft_malcolm`**.
- **IPv4 simple uniquement** pour le mandatory.
- 4 arguments, **toujours dans cet ordre** :
  1. source ip
  2. source mac address
  3. target ip
  4. target mac address
- Comportement : au lancement, **attendre** une requête ARP envoyée en broadcast par
  le *target* demandant la *source IP* → envoyer **une seule** ARP reply au target →
  **quitter**. Après coup, la table ARP du target doit contenir l'ip/mac fournies en source.
- **Ctrl+C** doit faire quitter proprement le programme.
- Gestion d'erreurs soignée : **aucun crash** (segfault, bus error, double free...).
  Messages d'erreur attendus pour IP invalide / host inconnu et MAC invalide.

## Contraintes de build / code

- **C** + **Makefile** (règles usuelles : all, clean, fclean, re).
- **Une seule variable globale** autorisée dans tout le projet.
- Organisation et nommage des fichiers libres.
- Correction **100% humaine** (soutenance entre pairs).

### Fonctions autorisées (mandatory)

`sendto`, `recvfrom`, `socket`, `setsockopt`, `inet_pton`, `inet_ntop`,
`if_nametoindex`, `sleep`, `getuid`, `close`, `sigaction`, `signal`, `inet_addr`,
`gethostbyname`, `getaddrinfo`, `freeaddrinfo`, `getifaddrs`, `freeifaddrs`,
`htons`, `ntohs`, `strerror` / `gai_strerror`, la famille `printf`, et les fonctions
**libft**. Autres fonctions tolérées pour le bonus **si justifiées en soutenance**.

## Bonus (évalué seulement si mandatory PARFAIT)

- Notation décimale pour adresses IPv4
- Résolution de hostname pour IPv4
- Mode verbose (affichage des infos du paquet)
- Toute autre fonctionnalité pertinente

> Le bonus n'est évalué **que si** le mandatory est intégralement fait et fonctionne
> sans défaut.

## Ressources locales

- `en.subject.pdf` — le sujet officiel (v3).
- `ft_malcolm_cours_complet.md` — notes de cours perso (couches réseau, Ethernet, ARP,
  table ARP, raw sockets, structure d'un paquet ARP, fonctions C, checklist).

## Notes de travail

- Privilèges root nécessaires (raw sockets) → `sudo ./ft_malcolm ...`.
- (à compléter au fil du projet : choix de design, pièges rencontrés, questions de soutenance)
