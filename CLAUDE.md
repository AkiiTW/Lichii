# Lichii V3-perso — notes de travail (Zelda Ocarina of Time remake, 0400486024DEC000)

Branche `claude/v3-zen2` du fork AkiiTW/Lichii (base : tag `frizti-v3`, 00ef0ea6c).
Builds : workflow `.github/workflows/lichii-tuned.yml` (MSYS2 clang zen2, saveurs normal / pgogen).
L'utilisateur remplace seulement `lichii.exe` dans `Desktop\lichii-windows-zen2-normal-43515fb…`.
PC : Ryzen 7 3800X (2 CCX : 0xff / 0xff00), RTX 5060 Ti 16 Go, 24 Go RAM, Windows 11.
Objectif : 30 FPS stables partout (mode 30 FPS par défaut), puis au-delà.

## Réglages / fichiers
- `%APPDATA%\eden\nx2_switches.txt` surcharge celui à côté de l'exe, qui surcharge `BuiltinSwitches` (src/yuzu/main.cpp).
  Actuellement : `EDEN_NX2_THREAD_USAGE=1`, `EDEN_NX2_GPU_PROFILE=1` (mesures).
- Journaux : `%APPDATA%\eden\log\eden_log.txt` (+ `.old.txt` = lancement précédent de Lichii).
- Mods : `%APPDATA%\eden\load\0400486024DEC000\` : `Dynamic-fps` (exefs, ne pas toucher),
  `OoT Perf - Ombres PNJ proches` (GARDER : LocalDepthShadowParam NPCFadeStart 25→8, NPCFadeEnd 30→10),
  `OoT Perf - Ombres 2 cascades` / `1 cascade` = À ABANDONNER (cascades≠3 → bug lumière arc-en-ciel dans la plaine, aucun gain).
- Dump du jeu : `C:\D\The Legend of Zelda Ocarina of Time [0400486024dec000][v131072]\{base,updates/131072}\romfs`.
  Outils BYML/zstd : scratchpad `oot/byml.py`, `oot/zs.py`, `oot/bympatch.py` (patch en place, taille identique → pas de RESTBL).

## Commits V3-perso
- 19538b31a pacer MinGW (#pragma → dwmapi), 43515fb50 30 FPS par défaut, eba617204 `EDEN_NX2_THREAD_USAGE`.
- Déjà dedans : fusion des clés de pipeline (attributs non lus, version de pilote), PREFLUSH retiré (flashs blancs).

## Mesures du 2026-11-07 (build eba6172, docked, 1x, FSR3)
Protocole : marché (tours de la fontaine caméra comme DT) puis course dans la plaine en regardant partout.
- Rendus/image au marché : A sans mod ~2 270 ; B PNJ proches ~2 100 ; C +2 cascades ~2 100 (bug) ; E portable ~2 100.
  → les fichiers du jeu ne donneront plus grand-chose. Le levier est Lichii.
- Marché ≈ 27-29 FPS, ~60 000 draws/s, 2,9 M méthodes/s, 4 500-6 000 render passes/s.
- Charge threads (100 % = 1 CPU logique) : GPU (front) 95 %, GPUBack 85 %, CPUCore_x 40-60 %, **HostTiming x2 98 % en permanence**.
- Front : 520 ms/s « split drain » = attente du back (fence release 288 ms à 1 463/s, macro dma read 216 ms à 2 010/s),
  macro 178 ms, resume pending 188 ms.
- **Back (goulot, ~880 ms/s)** : bind stage buffers 184 ms (125 k/s ≈ 1,5 µs), method 163 ms, other 109 ms,
  update buffers 62, draw 52, flush work 48, compute 46, pipeline lookup 35, fill image views 30, read handles 25,
  clear 25, dynamic state 19, fence 19. UBO comparés en place : 192 k inchangés / 109 k changés (par fenêtre).
  Méthodes chaudes : 0x8e4-0x8e6 (cb_data), 0x904/0x924 (cb bind), 0x586/0x585 (draw begin/end), 0x5f7/0x5f8.
- Present jamais bloqué par la fence GPU hôte → la carte graphique n'est pas le frein.
- Petit bug : après relance du jeu dans la même session Lichii, THREAD_USAGE ne logue plus les threads nommés.

## Pistes (ordre)
1. `BindHostStageBuffers` (buffer_cache) : coût par UBO ; sauter plus vite les UBO inchangés / éviter le travail répété par draw.
2. Attentes front→back aux fence release (1 463/s) et macro DMA reads.
3. Threads HostTiming à 98 % (attente active ?) → libérer deux CPU logiques.
4. Options V3 désactivées par défaut (EDEN_NX2_PERF_ON) : clean_words, index_rebind, bounds, submit_budget, fence_flush, fragment_barrier, front_sleep, async_pipelines.

## Déjà essayé sans gain
TC_NO_CAP (pire), DMA_READ_WAIT=track (pire), PREFLUSH=1 (flashs blancs), mods cascades (bug), mode portable (≈0).

## Veille : Claude de Frizti (captures envoyées par l'utilisateur)
Il travaille sur les attentes du front (lectures DMA des macros), nous sur le coût par rendu du back.
- 2026-11-07 14:53 — marché 34 FPS (objectif 50-60). Trouvé : ~1 000 attentes/s du thread GPU = une
  vérification qui abandonne après 512 écritures en attente (chez nous : compteurs « WaitForWritesTo gave up »
  / « DMA reads waited for unknown writes »). En test : index des écritures par page + suivi des envois
  inline. Ensuite : A/B de vitesse sur le parcours enregistré de Frizti.
- 2026-11-07 14:49 — marché 33-38 FPS. Analyse des ~2 000 attentes/s du front ; test d'un contrôle de
  recouvrement octet par octet (pas d'attente pour une écriture voisine sur la même page) ; ensuite suivre
  les plages écrites par les envois inline.
→ Ne pas surveiller son dépôt : l'utilisateur suit Frizti en direct (Discord) et dira quand intégrer ses commits. (gain estimé chez nous : jusqu'à ~+15 %, trous du back ~130 ms/s).

## Test de référence avec fenêtre (2026-11-07 14:55, build d8e4fe2, marché 150-270 s après chargement)
- ~21-22 FPS (frames 42-47 ms), ~45-48 k draws/s, ~2 100 draws/image ; GPUBack ~910 ms/s.
- Coûts par appel identiques au run E (20 min de jeu, 28-30 FPS). Différence : **texture upload 100-160 ms/s
  (~115/s, ~1 ms chacun)** et clears 55 ms/s (40 µs chacun vs 13) pendant toute la fenêtre → chargement
  des textures du marché dans les premières minutes après le lancement.
- Nouvelle piste : sortir les envois de textures de GPUBack (thread dédié) ou réduire leur coût CPU.
- Le protocole à fenêtre fixe rend les A/B comparables (même phase de chargement).

## Détail de « bind stage buffers » (2026-11-07 15:05, build 9842699, même fenêtre)
Par seconde au marché (~150 ms au total) :
- **storage binds ~66 ms** (~34 k/s, ~2 µs chacun) : SynchronizeBuffer sur toute la taille + BindMultiRangeStorage.
- **ubo bank ~33 ms** (~4 000/s, ~8 µs chacun, **~180 Mo/s** de banques 64 Kio ré-envoyées une fois par image).
- ubo fast ~21 ms (~240 k/s, ~500 o, 0,09 µs : déjà efficace) ; ubo classic ~13 ms (~46 k/s, ~28 Ko, ~900/s avec données CPU).
- reste de la zone ~13 ms. Texture upload toujours ~80-200 ms/s (~90-180/s) dans cette fenêtre.
Essais : 3) EDEN_NX2_PERF_ON=clean_words (aucune recompilation) ; ensuite code : comparer la banque avant de la ré-envoyer,
sous-zones dans TextureUpload, compteurs de taille des storage buffers.

## clean_words (2026-11-07 15:13) : GARDÉ
storage binds 64 → 17 ms/s ; draws/s +3 % ; frame inchangé (bruit textures + attentes front/back). Aucun défaut
visuel vu par l'utilisateur (marché). À vérifier un jour en forêt Kokiri (bug #161 suspecté à l'époque).
- 2026-11-07 15:18 — sa file de travail (LOOP_STATUS.md) : EXP10 (écritures des dispatches/uploads bornées, −6 % d'attente
  du front) ; ensuite releases de sémaphores/requêtes, drains de 14-18 ms aux fence release (ReleaseFences, puller.cpp),
  ~410 lectures/s « noted write ». Outils : parcours automatique fontaine (position de Link lue en mémoire, caméra fixe),
  replay_route.py, histogramme des lows 1 %/0,1 %, PGO réentraîné sur marché + plaine. Ports zortos : #465, #408, #463,
  #418, #411, #415, #232, #395/#337/#353. **Son point 13 = notre piste banques** (vérif. par bind ~10 % du back chez lui) :
  partager nos chiffres BANK_COMPARE / clean_words avec lui.

## Carnet partagé
https://github.com/AkiiTW/lichii-oot-notes (FINDINGS.md, WORK.md) : y ajouter chaque trouvaille ; lire les ajouts de Frizti.
Copie locale : scratchpad/notes (remote origin).

## Cache de textures (2026-11-07 16:38)
Cause : cibles de rendu recréées ~200/s (mémoire réutilisée), remplies avec la mémoire du jeu. EDEN_NX2_TC_RT_NO_UPLOAD=2 GARDÉ :
marché 48,6 → 40,4 ms/image, images >100 ms 42 → ~5. Reste : uploads de profondeur par un autre chemin (1,4-5 Go/600 images), clears ~70 ms/s,
garder les cibles aliasées en vie. Seuils TC plus grands (12288/13824) = pire (VRAM pleine).

## Test 9 (2026-11-07 17:18) : GARDÉ
EDEN_NX2_TC_RT_NO_UPLOAD=3 + EDEN_NX2_TC_RT_EVICT=4 (build 361d135) : texture upload 131 → 1,3 ms/s, marché 48,6 → 38,5 ms/image,
images >100 ms 42 → 1, VRAM 7,5-9 Go sous le seuil, 0 téléchargement. Ressenti : « bien meilleur, 21 FPS minimum ».
Suite : réutiliser les images hôtes par forme (~8 000 créations / 600 images), intégrer « acquire learning » de Frizti (+12,6 %).

## Piste 2, réutilisation des images hôtes (2026-11-07 19:02) : ABANDONNÉE
EDEN_NX2_TC_RT_REUSE (ca4f7d0 ; 0459584 ne faisait rien, MarkPoolable privé) : ~35 % des cibles réutilisées, réservoir plein à 1 Gio,
aucun gain (42,5 ms/image ; relance du code du test 9 : 40,8 ms → bruit ±2-3 ms), VRAM +1 Gio. Retiré des réglages.
Suite : PGO réentraîné sur le code actuel ; « acquire learning » et « frames-ahead 2 » de Frizti dès publication.

## PGO réentraîné (2026-11-07 20:28, c733486) : neutre, gardé
Profil marché + plaine + Kokiri sur ca4f7d0. Marché (4 derniers blocs) 40,9 ms/image vs 40,4-41,9 avec l'ancien profil ; travail du back
par rendu 13,8 µs vs 14,1-14,3. Dans le bruit. Un A/B d'un seul essai ne voit pas moins de ~5 % : faire plusieurs essais par côté.
