#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RetroQuiz — prototype terminal Windows
3 modes, 3 leaderboards séparés, pagination, navigation Z/A/M.

Lancer : python retroquiz_proto.py
Touches navigation leaderboard : fleche droite = page suiv, gauche = page prec
"""

import os, sys, json, time, random, msvcrt, textwrap
from pathlib import Path

# ── Configuration ─────────────────────────────────────────────────────────────
TEMPS_LIMITE_MS     = 10_000
TEMPS_LIMITE_ZEN_MS = 20_000
BAR_WIDTH           = 36
NB_QUESTIONS      = 10
MAX_PSEUDO        = 8
MAX_SCORES        = 10
ENTREES_PAR_PAGE  = 10
BASE_DIR          = Path(__file__).parent
FICHIER_QUESTIONS = BASE_DIR / "data" / "rq_questions.txt"

SCORES_FICHIER = {
    0: BASE_DIR / "rq_scores_zen.json",
    1: BASE_DIR / "rq_scores_arc.json",
    2: BASE_DIR / "rq_scores_ms.json",
}
NOM_MODE = {0: "ZEN", 1: "ARCADE", 2: "SURVIE"}

MODE_ZEN    = 0
MODE_ARCADE = 1
MODE_SURVIE = 2

# ── ANSI ──────────────────────────────────────────────────────────────────────
def _activer_ansi():
    try:
        import ctypes
        ctypes.windll.kernel32.SetConsoleMode(
            ctypes.windll.kernel32.GetStdHandle(-11), 7)
    except Exception:
        pass

def c(text, *codes):
    return f"\033[{';'.join(str(x) for x in codes)}m{text}\033[0m"

BG_BLUE = 44; JAUNE = 33; CYAN = 36; VERT = 32; ROUGE = 31; GRAS = 1
BG_VERT = 42; BG_JAUNE = 43; BG_ROUGE = 41; BG_NOIR = 40

# ── Utilitaires affichage ─────────────────────────────────────────────────────
def cls():
    os.system("cls")

def bandeau(droite=""):
    gauche = "  3615 RETROQUIZ"
    print(c(f"{gauche:<34}{droite:>6}", BG_BLUE, JAUNE))

def ligne_h(car="-"):
    print(car * 40)

def centrer_raw(texte, couleur=0):
    pad = max(0, (40 - len(texte)) // 2)
    print((" " * pad) + (c(texte, couleur) if couleur else texte))

# ── Questions ─────────────────────────────────────────────────────────────────
DEFAUT = [
    ("En quelle annee fut lance le Minitel ?",   ["1978","1982","1989","1995"], "B"),
    ("Quel numero pour acceder aux services ?",  ["3611","3614","3615","3617"], "C"),
    ("Largeur de l'ecran Minitel en colonnes ?", ["20","40","80","120"],        "B"),
    ("Quel composant est au coeur du Minimit ?", ["Raspberry Pi","Arduino Uno","ESP32","Atmel AVR"], "C"),
    ("Hauteur de l'ecran Minitel en lignes ?",   ["12","18","24","32"],         "C"),
    ("Quel protocole utilise le Minitel ?",      ["HTML","Videotex","FTP","Telnet"], "B"),
    ("En quelle annee le Minitel fut arrete ?",  ["2005","2009","2012","2017"], "C"),
    ("Qui a deploye le Minitel en France ?",     ["L ORTF","France Telecom","Minitel Club","Thomson"], "B"),
    ("A quelle vitesse le Minimit communique ?", ["1200 bauds","2400 bauds","4800 bauds","9600 bauds"], "A"),
    ("Que signifie ESP dans ESP32 ?",            ["Espressif","Extra Speed","Easy Serial","Echo Serial"], "A"),
]

def charger_questions():
    questions = []
    try:
        with open(FICHIER_QUESTIONS, encoding="utf-8") as f:
            for ligne in f:
                ligne = ligne.strip().lstrip("﻿")
                if not ligne or ligne.startswith("#"):
                    continue
                parts = ligne.split("|")
                if len(parts) == 6:
                    rep = parts[5].strip().upper()
                    if rep in "ABCD":
                        questions.append((parts[0].strip(),
                                          [p.strip() for p in parts[1:5]], rep))
    except FileNotFoundError:
        pass
    return questions or list(DEFAUT)

# ── Scores ────────────────────────────────────────────────────────────────────
def charger_scores(mode):
    try:
        data = json.loads(SCORES_FICHIER[mode].read_text(encoding="utf-8"))
        scores = [{"pseudo": str(s.get("pseudo","---"))[:MAX_PSEUDO],
                   "points": int(s.get("points", 0))} for s in data]
    except Exception:
        scores = []
    while len(scores) < MAX_SCORES:
        scores.append({"pseudo": "---", "points": 0})
    return scores[:MAX_SCORES]

def sauvegarder_scores(scores, mode):
    SCORES_FICHIER[mode].write_text(
        json.dumps(scores, indent=2, ensure_ascii=False), encoding="utf-8")

def inserer_score(scores, pseudo, points, mode):
    scores.append({"pseudo": pseudo[:MAX_PSEUDO].upper(), "points": points})
    scores.sort(key=lambda s: s["points"], reverse=True)
    del scores[MAX_SCORES:]
    sauvegarder_scores(scores, mode)

# ── Clavier ───────────────────────────────────────────────────────────────────
def vider_buffer():
    while msvcrt.kbhit():
        msvcrt.getwch()

def lire_touche():
    """Retourne un caractere ou une chaine nommee pour les touches speciales."""
    ch = msvcrt.getwch()
    if ch in ("\x00", "\xe0"):
        code = msvcrt.getwch()
        if code == "\x4d": return "SUITE"    # fleche droite
        if code == "\x4b": return "RETOUR"   # fleche gauche
        if code == "\x48": return "RETOUR"   # fleche haut
        if code == "\x50": return "SUITE"    # fleche bas
        return ""
    return ch

def attendre_touche():
    vider_buffer()
    while True:
        ch = lire_touche()
        if ch:
            return ch if ch in ("SUITE","RETOUR") else ch.upper()

def saisir_pseudo():
    buf = ""
    sys.stdout.write("  Pseudo (ENTREE pour valider) : ")
    sys.stdout.flush()
    while True:
        ch = lire_touche()
        if not ch or ch in ("SUITE","RETOUR"):
            continue
        if ch in ("\r", "\n"):
            print()
            return buf
        if ch == "\x08" and buf:
            buf = buf[:-1]
            sys.stdout.write(f"\r  Pseudo (ENTREE pour valider) : {buf} \r"
                             f"  Pseudo (ENTREE pour valider) : {buf}")
            sys.stdout.flush()
        elif len(buf) < MAX_PSEUDO and ch.isprintable():
            buf += ch
            sys.stdout.write(ch)
            sys.stdout.flush()

# ── Question ──────────────────────────────────────────────────────────────────
def _split_choix(text, maxlen=15):
    if len(text) <= maxlen:
        return text, ""
    cut = maxlen
    while cut > 0 and text[cut] != ' ':
        cut -= 1
    if cut == 0:
        cut = maxlen
    return text[:cut], text[cut:].lstrip()[:maxlen]

def afficher_question(enonce, choix, num, mode, total=None):
    if total is None:
        total = NB_QUESTIONS
    # 1. Enonce seul (word-wrap, sans cadre)
    cls()
    bandeau(f"[{num:02d}/{total:02d}]")
    print()
    for ligne_enonce in textwrap.wrap(enonce, 38):
        print(c(f"  {ligne_enonce}", CYAN))
    print()
    ligne_h()
    sys.stdout.flush()
    # 2. Pause lecture 2s
    time.sleep(2.0)
    # 3. Choix 2 lignes × 2 colonnes + instruction
    for row in range(2):
        idxL = row * 2
        idxR = row * 2 + 1
        l1L, l2L = _split_choix(choix[idxL])
        l1R, l2R = _split_choix(choix[idxR])
        print(f"  {idxL+1}/ {l1L:<15}  {idxR+1}/ {l1R:<15}")
        print(f"     {l2L:<15}     {l2R:<15}")
        if row == 0:
            print()
    print()
    if mode == MODE_ARCADE:
        print("  Appuyez sur 1, 2, 3 ou 4")
    else:
        print("  Tapez 1, 2, 3 ou 4 puis ENTREE")
    print()

DIGIT_TO_LETTRE = {"1": "A", "2": "B", "3": "C", "4": "D"}
LETTRE_TO_DIGIT = {v: k for k, v in DIGIT_TO_LETTRE.items()}

def highlight_reponse(digit, choix_list):
    def fmt(d, l1, l2, selected):
        p, s = ("\033[7m", "\033[0m") if selected else ("", "")
        return (f"{p}  {d}/ {l1:<15}{s}", f"{p}     {l2:<15}{s}")

    lL0, l2L0 = _split_choix(choix_list[0])
    lL1, l2L1 = _split_choix(choix_list[1])
    lL2, l2L2 = _split_choix(choix_list[2])
    lL3, l2L3 = _split_choix(choix_list[3])

    r0L1_l, r0L2_l = fmt('1', lL0, l2L0, digit == '1')
    r0L1_r, r0L2_r = fmt('2', lL1, l2L1, digit == '2')
    r1L1_l, r1L2_l = fmt('3', lL2, l2L2, digit == '3')
    r1L1_r, r1L2_r = fmt('4', lL3, l2L3, digit == '4')

    sys.stdout.write(
        f"\033[9A\r\033[2K{r0L1_l}{r0L1_r}"
        f"\n\033[2K{r0L2_l}{r0L2_r}"
        f"\033[2B\r\033[2K{r1L1_l}{r1L1_r}"
        f"\n\033[2K{r1L2_l}{r1L2_r}"
        f"\033[5B\r"
    )
    sys.stdout.flush()

def _barre_timer(elapsed_ms, limit=TEMPS_LIMITE_MS, width=BAR_WIDTH):
    remaining = max(0, limit - elapsed_ms)
    filled = int(width * remaining / limit)
    if   remaining > limit * 0.7: bg = BG_VERT
    elif remaining > limit * 0.3: bg = BG_JAUNE
    else:                         bg = BG_ROUGE
    vide  = " " * (width - filled)
    plein = f"\033[{bg}m" + " " * filled + "\033[0m"
    return f"  {vide}{plein}"

def jouer_question_arcade(enonce, choix, bonne, num):
    afficher_question(enonce, choix, num, MODE_ARCADE)
    time.sleep(1.0)
    t0 = time.perf_counter()
    dernier_bar_width = -1
    vider_buffer()

    while True:
        elapsed_ms = int((time.perf_counter() - t0) * 1000)
        remaining  = max(0, TEMPS_LIMITE_MS - elapsed_ms)
        bar_width  = int(BAR_WIDTH * remaining / TEMPS_LIMITE_MS)

        if bar_width != dernier_bar_width:
            dernier_bar_width = bar_width
            sys.stdout.write(f"\r{_barre_timer(elapsed_ms)}")
            sys.stdout.flush()

        if elapsed_ms >= TEMPS_LIMITE_MS:
            print()
            return None, elapsed_ms

        if msvcrt.kbhit():
            ch = lire_touche()
            if ch in "1234":
                print()
                highlight_reponse(ch, choix)
                return ch, int((time.perf_counter() - t0) * 1000)

        time.sleep(0.03)

def jouer_question_zen(enonce, choix, bonne, num, total=None):
    afficher_question(enonce, choix, num, MODE_ZEN, total=total)
    time.sleep(2.0)
    t0 = time.perf_counter()
    dernier_bar_width = -1
    saisie = ""
    vider_buffer()

    while True:
        elapsed_ms = int((time.perf_counter() - t0) * 1000)
        remaining  = max(0, TEMPS_LIMITE_ZEN_MS - elapsed_ms)
        bar_width  = int(BAR_WIDTH * remaining / TEMPS_LIMITE_ZEN_MS)

        if bar_width != dernier_bar_width:
            dernier_bar_width = bar_width
            ind = saisie if saisie else ' '
            sys.stdout.write(
                f"\r  [{ind}]{_barre_timer(elapsed_ms, TEMPS_LIMITE_ZEN_MS)}  ENTREE=valider  "
            )
            sys.stdout.flush()

        if elapsed_ms >= TEMPS_LIMITE_ZEN_MS:
            print()
            return None, elapsed_ms

        if msvcrt.kbhit():
            ch = lire_touche()
            if ch in "1234":
                saisie = ch
                sys.stdout.write("\n")
                highlight_reponse(ch, choix)
                sys.stdout.write("\033[A")
                sys.stdout.flush()
            elif ch in ("\r", "\n") and saisie:
                print()
                return saisie, int((time.perf_counter() - t0) * 1000)

        time.sleep(0.03)

# ── Partie ────────────────────────────────────────────────────────────────────
def demarrer_partie(questions, mode):
    if mode == MODE_SURVIE:
        tirage   = random.sample(questions, len(questions))
        nb_total = len(questions)
    else:
        if len(questions) < NB_QUESTIONS:
            cls()
            print(c(f"\n  Besoin de {NB_QUESTIONS} questions min "
                    f"({len(questions)} disponibles).", ROUGE))
            time.sleep(2)
            return
        tirage   = random.sample(questions, NB_QUESTIONS)
        nb_total = NB_QUESTIONS

    score = 0

    for i, (enonce, choix, bonne) in enumerate(tirage):
        bonne_digit = LETTRE_TO_DIGIT.get(bonne, "?")

        if mode == MODE_ARCADE:
            digit, elapsed_ms = jouer_question_arcade(enonce, choix, bonne, i + 1)
            reponse = DIGIT_TO_LETTRE.get(digit) if digit else None
        else:
            digit, elapsed_ms = jouer_question_zen(enonce, choix, bonne, i + 1, total=nb_total)
            reponse = DIGIT_TO_LETTRE.get(digit) if digit else None

        if reponse is None:
            print(c("  TEMPS ECOULE !  (0 pt)", ROUGE))
            time.sleep(1.3)
        elif reponse == bonne:
            if mode == MODE_ARCADE:
                pts = max(0, (TEMPS_LIMITE_MS - elapsed_ms) // 100)
                score += pts
                print(c(f"  BONNE REPONSE !  +{pts:3d} pts", VERT))
            elif mode == MODE_ZEN:
                pts = max(0, (TEMPS_LIMITE_ZEN_MS - elapsed_ms) // 200)
                score += pts
                print(c(f"  BONNE REPONSE !  +{pts:4d} pts", VERT))
            else:
                score += 1
                print(c("  BONNE REPONSE !", VERT))
            time.sleep(1.3)
        else:
            print(c(f"  FAUX ! Bonne rep : {bonne_digit}   (0 pt)", ROUGE))
            time.sleep(1.3)
            if mode == MODE_SURVIE:
                break  # fin immediatediate a la premiere mauvaise reponse

    # Fin de partie
    cls()
    bandeau()
    print()
    print()
    if mode == MODE_ARCADE:
        centrer_raw(f"Score : {score} / {100 * nb_total} pts", JAUNE)
    elif mode == MODE_ZEN:
        centrer_raw(f"Score : {score} / {100 * nb_total} pts", JAUNE)
    elif mode == MODE_SURVIE:
        centrer_raw(f"Score : {score} bonnes reponses", JAUNE)
    print()
    print()
    pseudo = saisir_pseudo()
    if not pseudo.strip():
        pseudo = "ANONYME"

    scores = charger_scores(mode)
    inserer_score(scores, pseudo, score, mode)

    # Afficher le leaderboard du mode joué
    afficher_leaderboard(mode, 0)

# ── Leaderboard ───────────────────────────────────────────────────────────────
def afficher_leaderboard(mode, page):
    while True:
        scores = charger_scores(mode)
        total_pages = max(1, (MAX_SCORES + ENTREES_PAR_PAGE - 1) // ENTREES_PAR_PAGE)
        page = max(0, min(page, total_pages - 1))
        debut = page * ENTREES_PAR_PAGE

        cls()
        bandeau()
        print()
        centrer_raw(f"** {NOM_MODE[mode]} **", JAUNE)
        ligne_h()
        for i in range(ENTREES_PAR_PAGE):
            idx = debut + i
            if idx < MAX_SCORES:
                s = scores[idx]
                print(f"  {idx+1:2d}.  {s['pseudo']:<8}    {s['points']:4d} pts")
        print()
        print(f"  Page {page+1} / {total_pages}")
        print()
        print(c("  Z=ZEN  A=ARCADE  S=SURVIE  J=Rejouer", CYAN))
        if total_pages > 1:
            print(c("  -> page suiv   <- page prec", CYAN))
        print()
        ligne_h()
        print(c("  0 = accueil", CYAN))

        ch = attendre_touche()

        if ch == "0":
            return
        elif ch == "SUITE" and page < total_pages - 1:
            page += 1
        elif ch == "RETOUR" and page > 0:
            page -= 1
        elif ch == "Z":
            mode, page = MODE_ZEN, 0
        elif ch == "A":
            mode, page = MODE_ARCADE, 0
        elif ch == "S":
            mode, page = MODE_SURVIE, 0
        elif ch == "J":
            demarrer_partie(charger_questions(), mode)
            return

# ── Navigation leaderboards ───────────────────────────────────────────────────
def afficher_nav_leaderboards():
    while True:
        cls()
        bandeau()
        print()
        centrer_raw("** CLASSEMENTS **", CYAN)
        print()
        ligne_h("=")
        print()
        print("  " + c("Z", VERT, 4) + c("EN       - scores classiques", VERT))
        print()
        print("  " + c("A", VERT, 4) + c("RCADE    - scores par rapidite", VERT))
        print()
        print("  " + c("S", VERT, 4) + c("URVIE - survie", VERT))
        print()
        print()
        print("  Tapez Z, A ou S")
        ligne_h()
        print(c("  0 = accueil", CYAN))

        ch = attendre_touche()
        if ch == "Z":
            afficher_leaderboard(MODE_ZEN, 0)
            return
        elif ch == "A":
            afficher_leaderboard(MODE_ARCADE, 0)
            return
        elif ch == "S":
            afficher_leaderboard(MODE_SURVIE, 0)
            return
        elif ch == "0":
            return

# ── Selection de mode ─────────────────────────────────────────────────────────
def afficher_selection_mode():
    while True:
        cls()
        bandeau()
        print()
        centrer_raw("** CHOISISSEZ UN MODE **", CYAN)
        print()
        ligne_h("=")
        print()
        print("  " + c("Z", VERT, 4) + c("EN       - Quiz classique", VERT))
        print()
        print("  " + c("A", VERT, 4) + c("RCADE    - 10s par question !", VERT))
        print()
        print("  " + c("S", VERT, 4) + c("URVIE", VERT))
        print()
        print()
        print("  Tapez votre choix")
        ligne_h()
        print(c("  0 = accueil", CYAN))

        ch = attendre_touche()
        if ch == "Z":
            return MODE_ZEN
        elif ch == "A":
            return MODE_ARCADE
        elif ch == "S":
            return MODE_SURVIE
        elif ch == "0":
            return None

# ── Admin ─────────────────────────────────────────────────────────────────────
MDP_ADMIN = "1234"

def sauvegarder_questions(questions):
    try:
        FICHIER_QUESTIONS.parent.mkdir(exist_ok=True)
        with open(FICHIER_QUESTIONS, 'w', encoding='utf-8') as f:
            for enonce, choix, rep in questions:
                e = enonce.replace('|', '/').replace('\n', ' ')
                c_list = [str(x).replace('|', '/').replace('\n', ' ') for x in choix]
                f.write(f"{e}|{'|'.join(c_list)}|{rep}\n")
        return True
    except Exception:
        return False

def admin_demander_mdp():
    cls()
    bandeau()
    print()
    centrer_raw("** ESPACE ADMIN **", CYAN)
    print()
    print()
    try:
        import getpass
        pwd = getpass.getpass("  Mot de passe : ")
    except Exception:
        pwd = input("  Mot de passe : ")
    if pwd == MDP_ADMIN:
        return True
    print(c("  Mot de passe incorrect !", ROUGE))
    time.sleep(1.5)
    return False

def afficher_admin_reset():
    while True:
        cls()
        bandeau()
        print()
        centrer_raw("** REINITIALISER CLASSEMENT **", CYAN)
        print()
        ligne_h("=")
        print()
        print(c("Z", VERT, 4) + c("EN", VERT))
        print()
        print(c("A", VERT, 4) + c("RCADE", VERT))
        print()
        print(c("M", VERT, 4) + c("ORT SUBITE", VERT))
        print()
        print(c("T", VERT, 4) + c("OUS", VERT))
        print()
        print()
        print("  Tapez votre choix")
        ligne_h()
        print(c("  0 = menu admin", CYAN))
        ch = attendre_touche()
        if ch == "0":
            return
        modes_map = {"Z": [MODE_ZEN], "A": [MODE_ARCADE],
                     "S": [MODE_SURVIE],
                     "T": [MODE_ZEN, MODE_ARCADE, MODE_SURVIE]}
        if ch in modes_map:
            vide = [{"pseudo": "---", "points": 0} for _ in range(MAX_SCORES)]
            for m in modes_map[ch]:
                sauvegarder_scores(vide[:], m)
            print(c("  Classement(s) reinitialise(s) !", VERT))
            time.sleep(1.3)
            return

def afficher_admin_liste():
    NB_PAR_PAGE = 5
    page = 0
    while True:
        questions = charger_questions()
        total_pages = max(1, (len(questions) + NB_PAR_PAGE - 1) // NB_PAR_PAGE)
        page = max(0, min(page, total_pages - 1))
        debut = page * NB_PAR_PAGE
        cls()
        bandeau()
        print()
        centrer_raw("** QUESTIONS **", JAUNE)
        ligne_h()
        for i in range(NB_PAR_PAGE):
            idx = debut + i
            if idx < len(questions):
                enonce, _, _ = questions[idx]
                print(f"  {idx+1:2d}. {enonce[:33]}")
            else:
                print()
        print()
        print(f"  Page {page+1}/{total_pages}  ({len(questions)} questions)")
        print()
        ligne_h()
        print(c("  0=menu  ->=suiv  <=prec  chiffre=selectionner", CYAN))
        ch = attendre_touche()
        if ch == "0":
            return
        elif ch == "SUITE" and page < total_pages - 1:
            page += 1
        elif ch == "RETOUR" and page > 0:
            page -= 1
        elif ch.isdigit():
            vider_buffer()
            num_str = ch
            sys.stdout.write(f"\r  Numero : {num_str}  ")
            sys.stdout.flush()
            while True:
                c2 = lire_touche()
                if c2 in ("\r", "\n"):
                    print()
                    break
                elif c2 == "\x08":
                    if num_str:
                        num_str = num_str[:-1]
                        sys.stdout.write(f"\r  Numero : {num_str}   ")
                        sys.stdout.flush()
                elif c2.isdigit() and len(num_str) < 3:
                    num_str += c2
                    sys.stdout.write(c2)
                    sys.stdout.flush()
            if num_str:
                n = int(num_str)
                if 1 <= n <= len(questions):
                    admin_editer_question(n - 1)

def admin_editer_question(idx):
    questions = charger_questions()
    if idx >= len(questions):
        return
    enonce, choix, rep = questions[idx]
    choix = list(choix)
    labels = ["Enonce", "Choix A", "Choix B", "Choix C", "Choix D", "Bonne rep (A-D)"]
    for f in range(6):
        current = ([enonce] + choix + [rep])[f]
        cls()
        bandeau()
        print()
        centrer_raw("** MODIFIER QUESTION **", CYAN)
        print()
        print(f"  Question {idx+1} / {len(questions)}")
        print()
        print(c(f"  {labels[f]} :", CYAN))
        print(f"  {current}")
        print()
        print("  Nouveau (vide = conserver) :")
        while True:
            val = input("  > ").strip()
            if not val:
                break
            if f == 5:
                val = val[0].upper()
                if val in "ABCD":
                    rep = val
                    break
                print(c("  (A, B, C ou D requis)", ROUGE))
            elif f == 0:
                enonce = val
                break
            else:
                choix[f - 1] = val
                break
    questions[idx] = (enonce, choix, rep)
    sauvegarder_questions(questions)
    print(c("  Sauvegarde OK !", VERT))
    time.sleep(1.2)

def admin_ajouter_question():
    cls()
    bandeau()
    print()
    centrer_raw("** AJOUTER QUESTION **", CYAN)
    print()
    champs = ["Enonce", "Choix A", "Choix B", "Choix C", "Choix D"]
    valeurs = []
    for champ in champs:
        while True:
            val = input(f"  {champ} : ").strip()
            if val:
                valeurs.append(val)
                break
            print(c("  (obligatoire)", ROUGE))
    while True:
        rep = input("  Bonne reponse (A/B/C/D) : ").strip()
        if rep and rep[0].upper() in "ABCD":
            rep = rep[0].upper()
            break
        print(c("  (A, B, C ou D)", ROUGE))
    questions = charger_questions()
    questions.append((valeurs[0], valeurs[1:5], rep))
    sauvegarder_questions(questions)
    print(c("  Question ajoutee !", VERT))
    time.sleep(1.5)

def afficher_admin_menu():
    while True:
        cls()
        bandeau()
        print()
        centrer_raw("** ADMIN **", CYAN)
        print()
        ligne_h("=")
        print()
        print(c("  [ 1 ]  Reinitialiser classements", VERT))
        print()
        print(c("  [ 2 ]  Modifier une question", VERT))
        print()
        print(c("  [ 3 ]  Ajouter une question", VERT))
        print()
        print()
        print("  Tapez votre choix")
        ligne_h()
        print(c("  0 = accueil", CYAN))
        ch = attendre_touche()
        if ch == "0":
            return
        elif ch == "1":
            afficher_admin_reset()
        elif ch == "2":
            afficher_admin_liste()
        elif ch == "3":
            admin_ajouter_question()

# ── Accueil ───────────────────────────────────────────────────────────────────
def afficher_accueil(nb_questions):
    cls()
    bandeau()
    print()
    print()
    centrer_raw("** 3615 RETROQUIZ **", CYAN)
    print()
    print("  Le quiz du retrogaming !")
    print()
    ligne_h("=")
    print()
    print(c("  [ 1 ]  JOUER AU QUIZ", VERT))
    print()
    print(c("  [ 2 ]  CLASSEMENTS", VERT))
    print()
    print(c("  [ 3 ]  ADMIN", VERT))
    print()
    print()
    print("  Tapez votre choix")
    ligne_h()
    print(c(f"  Q = quitter   ({nb_questions} questions dispo)", CYAN))

# ── Main ──────────────────────────────────────────────────────────────────────
def main():
    _activer_ansi()
    questions = charger_questions()

    while True:
        afficher_accueil(len(questions))
        ch = attendre_touche()

        if ch == "Q":
            cls()
            print("Au revoir !")
            break
        elif ch == "1":
            mode = afficher_selection_mode()
            if mode is not None:
                demarrer_partie(questions, mode)
        elif ch == "2":
            afficher_nav_leaderboards()
        elif ch == "3":
            if admin_demander_mdp():
                afficher_admin_menu()

if __name__ == "__main__":
    main()
