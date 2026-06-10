// ============================================================
//  RETROQUIZ.INO — VERSION DEBUG
//  Affiche dans le Moniteur Serie ce qui se passe exactement.
//  Ouvrir le moniteur serie a 115200 bauds pendant le test.
// ============================================================

#include "rq_donnees.h"

#define RQ_ACCUEIL     0
#define RQ_JEU         1
#define RQ_LEADERBOARD 2
#define RQ_ADMIN_MENU  3
#define RQ_ADMIN_RESET 4
#define RQ_ADMIN_LISTE 5
#define RQ_SELECTION_MODE   6
#define RQ_LEADERBOARDS_NAV 7
#define RQ_MAX_BANQUE 100

#define RQ_MODE_ZEN         0
#define RQ_MODE_ARCADE      1
#define RQ_MODE_SURVIE 2

#define RQ_TEMPS_LIMITE_ZEN_MS 20000

static int rq_etat = RQ_ACCUEIL;

static RQ_Question rq_banque[RQ_MAX_BANQUE];
static int         rq_totalBanque = 0;
static int         rq_indices[RQ_MAX_BANQUE];
static int         rq_nbQuestions = RQ_NB_QUESTIONS_PARTIE;
static int         rq_questionNum = 0;
static int         rq_score       = 0;
static char        rq_reponse     = 0;
static char        rq_pseudo[RQ_MAX_PSEUDO + 1] = "";
static int           rq_pageAdmin         = 0;
static RQ_Question   rq_editQ;
static unsigned long rq_questionStartTime  = 0;
static bool          rq_timedOut           = false;
static int           rq_mode               = RQ_MODE_ZEN;
static int           rq_lbMode             = RQ_MODE_ZEN;
static int           rq_lbPage             = 0;
static int           rq_adminEditIdx       = 0;
static unsigned long rq_responseTime       = 0;

static unsigned long rq_getTimeLimitMs() {
    return (rq_mode == RQ_MODE_ZEN)
           ? (unsigned long)RQ_TEMPS_LIMITE_ZEN_MS
           : (unsigned long)RQ_TEMPS_LIMITE_MS;
}

// ============================================================
// UTILITAIRES
// ============================================================
static void rq_centrer(const String& txt, int y) {
    int pad = (40 - (int)txt.length()) / 2;
    if (pad < 0) pad = 0;
    minitel.newXY(1 + pad, y);
    minitel.print(txt);
}

static void rq_ligneH(int y, char c = '-') {
    minitel.newXY(1, y);
    for (int i = 0; i < 40; i++) minitel.print(String(c));
}

static void rq_bandeau(int numQ = 0) {
    minitel.newXY(1, 1);
    minitel.attributs(FOND_BLEU);
    minitel.attributs(CARACTERE_JAUNE);
    if (numQ > 0) {
        char buf[41];
        sprintf(buf, "3615 RETROQUIZ              [%02d/%02d]",
                numQ, rq_nbQuestions);
        minitel.print(buf);
    } else {
        minitel.print("3615 RETROQUIZ                        ");
    }
    minitel.attributs(FOND_NOIR);
    minitel.attributs(CARACTERE_BLANC);
}

static void rq_pied(const String& aide) {
    rq_ligneH(23);
    minitel.newXY(1, 24);
    minitel.attributs(CARACTERE_CYAN);
    minitel.print(aide);
    minitel.attributs(CARACTERE_BLANC);
}

static void rq_afficherEnonce(const String& texte) {
    const int LARGEUR = 38;
    int debut = 0, len = (int)texte.length(), ligne = 3;
    while (debut < len && ligne <= 6) {
        int fin = debut + LARGEUR;
        String portion;
        if (fin >= len) {
            portion = texte.substring(debut);
            debut = len;
        } else {
            int coupe = fin;
            while (coupe > debut && texte.charAt(coupe) != ' ') coupe--;
            if (coupe == debut) coupe = fin;
            portion = texte.substring(debut, coupe);
            debut = (texte.charAt(coupe) == ' ') ? coupe + 1 : coupe;
        }
        minitel.newXY(1, ligne++);
        minitel.attributs(CARACTERE_CYAN);
        minitel.print("  " + portion);
        minitel.attributs(CARACTERE_BLANC);
    }
}

static void rq_splitChoix(const String& text, String& l1, String& l2) {
    int len = (int)text.length();
    if (len <= 15) { l1 = text; l2 = ""; return; }
    int cut = 15;
    while (cut > 0 && text.charAt(cut) != ' ') cut--;
    if (cut == 0) cut = 15;
    l1 = text.substring(0, cut);
    int start2 = (text.charAt(cut) == ' ') ? cut + 1 : cut;
    l2 = text.substring(start2);
    if ((int)l2.length() > 15) l2 = l2.substring(0, 15);
}

static void rq_highlightReponse(int idx) {
    RQ_Question& q = rq_banque[rq_indices[rq_questionNum]];
    for (int row = 0; row < 2; row++) {
        int idxL = row * 2;
        int idxR = row * 2 + 1;
        int y    = 9 + row * 3;
        String l1L, l2L, l1R, l2R;
        rq_splitChoix(q.choix[idxL], l1L, l2L);
        rq_splitChoix(q.choix[idxR], l1R, l2R);
        char b1L[21], b2L[21], b1R[21], b2R[21];
        sprintf(b1L, "  %c/ %-15s", '1'+idxL, l1L.c_str());
        sprintf(b2L, "     %-15s",            l2L.c_str());
        sprintf(b1R, "  %c/ %-15s", '1'+idxR, l1R.c_str());
        sprintf(b2R, "     %-15s",            l2R.c_str());

        minitel.newXY(1, y);
        if (idx >= 0 && idxL == idx) minitel.attributs(INVERSION_FOND);
        minitel.print(b1L);
        minitel.attributs(FOND_NOIR); minitel.attributs(CARACTERE_BLANC);
        minitel.newXY(21, y);
        if (idx >= 0 && idxR == idx) minitel.attributs(INVERSION_FOND);
        minitel.print(b1R);
        minitel.attributs(FOND_NOIR); minitel.attributs(CARACTERE_BLANC);

        minitel.newXY(1, y+1);
        if (idx >= 0 && idxL == idx) minitel.attributs(INVERSION_FOND);
        minitel.print(b2L);
        minitel.attributs(FOND_NOIR); minitel.attributs(CARACTERE_BLANC);
        minitel.newXY(21, y+1);
        if (idx >= 0 && idxR == idx) minitel.attributs(INVERSION_FOND);
        minitel.print(b2R);
        minitel.attributs(FOND_NOIR); minitel.attributs(CARACTERE_BLANC);
    }
}

static void rq_afficherQuestion(int numQ) {
    RQ_Question& q = rq_banque[rq_indices[numQ]];
    minitel.newScreen();
    rq_bandeau(numQ + 1);
    rq_afficherEnonce(q.enonce);
    rq_ligneH(7);
    delay(2000);
    for (int row = 0; row < 2; row++) {
        int idxL = row * 2;
        int idxR = row * 2 + 1;
        int y    = 9 + row * 3;
        String l1L, l2L, l1R, l2R;
        rq_splitChoix(q.choix[idxL], l1L, l2L);
        rq_splitChoix(q.choix[idxR], l1R, l2R);
        char buf1[41], buf2[41];
        sprintf(buf1, "  %c/ %-15s  %c/ %-15s", '1'+idxL, l1L.c_str(), '1'+idxR, l1R.c_str());
        sprintf(buf2, "     %-15s     %-15s",    l2L.c_str(), l2R.c_str());
        minitel.newXY(1, y);   minitel.print(buf1);
        minitel.newXY(1, y+1); minitel.print(buf2);
    }
    rq_pied("  RET=Abandonner");
    rq_reponse = 0;
    currentEcran = "RQ_JEU";
}

static void rq_afficherBarreTimer(unsigned long elapsed, int barWidth, unsigned long limitMs, bool reset = false) {
    static int dernierBarWidth = -1;
    static int derniereCouleur = -1;

    // Si reset demandé, réinitialiser les variables statiques
    if (reset) {
        dernierBarWidth = -1;
        derniereCouleur = -1;
        return;
    }

    // Calculer le temps restant et la couleur correspondante
    unsigned long remaining = (elapsed < limitMs) ? limitMs - elapsed : 0;
    int couleurFond;
    if      (remaining > limitMs * 7 / 10) couleurFond = FOND_VERT;   // Plus de 70% restant
    else if (remaining > limitMs * 3 / 10) couleurFond = FOND_JAUNE;  // Entre 30% et 70% restant
    else                                    couleurFond = FOND_ROUGE;   // Moins de 30% restant

    minitel.noCursor();

    // Premier affichage : afficher la barre complète
    if (dernierBarWidth == -1) {
        minitel.newXY(3, 21);
        minitel.attributs(couleurFond);
        for (int i = 0; i < 36; i++) minitel.print(" ");
        derniereCouleur = couleurFond;
        dernierBarWidth = 36;
    }
    // Changement de couleur : réafficher toute la barre restante
    else if (couleurFond != derniereCouleur && barWidth > 0) {
        minitel.newXY(3, 21);
        minitel.attributs(couleurFond);
        for (int i = 0; i < barWidth; i++) minitel.print(" ");
        derniereCouleur = couleurFond;
    }
    // Décroissance : effacer les caractères de droite
    else if (barWidth < dernierBarWidth) {
        minitel.attributs(FOND_NOIR);
        for (int i = barWidth; i < dernierBarWidth; i++) {
            minitel.newXY(3 + i, 21);
            minitel.print(" ");
        }
    }

    dernierBarWidth = barWidth;

    minitel.attributs(FOND_NOIR);
    minitel.attributs(CARACTERE_BLANC);
    minitel.newXY(1, 22);
    minitel.cursor();
}

static void rq_attendreAccueil() {
    touche = 0;
    userInput = "";
    minitel.echo(false);
    minitel.newXY(1, 22);
    minitel.cursor();

    while (true) {
        unsigned long k = minitel.getKeyCode();
        if (k == 0) continue;

        if (k == CONNEXION_FIN || k == SOMMAIRE) {
            touche = k;
            lasttouche = k;
            minitel.noCursor();
            minitel.echo(true);
            return;
        } else if (k == 'J' || k == 'j' || k == 'C' || k == 'c' || k == 'A' || k == 'a') {
            char ch = toupper((char)k);
            userInput = String(ch);
            lasttouche = k;
            touche = k;
            minitel.noCursor();
            minitel.echo(true);
            return;
        }
    }
}

static void rq_attendreSelectionMode() {
    touche = 0;
    userInput = "";
    minitel.echo(false);
    minitel.newXY(1, 22);
    minitel.cursor();

    while (true) {
        unsigned long k = minitel.getKeyCode();
        if (k == 0) continue;

        if (k == CONNEXION_FIN || k == SOMMAIRE || k == RETOUR) {
            touche = k;
            lasttouche = k;
            minitel.noCursor();
            minitel.echo(true);
            return;
        } else if (k == 'Z' || k == 'z' || k == 'A' || k == 'a' || k == 'S' || k == 's') {
            char ch = toupper((char)k);
            userInput = String(ch);
            lasttouche = k;
            touche = k;
            minitel.noCursor();
            minitel.echo(true);
            return;
        }
    }
}

static void rq_attendreReponseJeu() {
    rq_timedOut = false;
    touche = 0;
    userInput = "";
    minitel.echo(false);
    int dernierBarWidth = -1;
    unsigned long limitMs = rq_getTimeLimitMs();
    while (true) {
        if (rq_mode == RQ_MODE_ARCADE || rq_mode == RQ_MODE_ZEN) {
            unsigned long elapsed = millis() - rq_questionStartTime;
            unsigned long remaining = (elapsed < limitMs) ? limitMs - elapsed : 0;
            // Calcul décroissant : basé sur le temps restant
            int barWidth = (int)((36L * (long)remaining + limitMs / 2) / limitMs);
            if (barWidth > 36) barWidth = 36;  // Limiter à 36 caractères max
            if (barWidth != dernierBarWidth) {
                dernierBarWidth = barWidth;
                rq_afficherBarreTimer(elapsed, barWidth, limitMs);
            }
            if (elapsed >= limitMs) {
                rq_timedOut = true;
                minitel.noCursor();
                minitel.echo(true);
                return;
            }
        }
        unsigned long k = (rq_mode == RQ_MODE_SURVIE)
                          ? minitel.getKeyCode()
                          : minitel.getKeyCode(false);
        if (k == 0) continue;
        if (k == CONNEXION_FIN || k == SOMMAIRE || k == RETOUR || k == SUITE || k == ENVOI) {
            touche = k;
            lasttouche = k;
            minitel.noCursor();
            minitel.echo(true);
            return;
        } else if (k >= '1' && k <= '4') {
            char ch = (char)k;
            userInput = String(ch);
            lasttouche = k;
            touche = k;
            minitel.newXY(1, 22);
            minitel.print(String(ch));
            rq_responseTime = millis();  // Capturer le temps AVANT l'inversion vidéo
            rq_highlightReponse(ch - '1');
            if (rq_mode == RQ_MODE_ARCADE) {
                touche = ENVOI;
                minitel.noCursor();
                minitel.echo(true);
                return;
            }
        }
    }
}

static unsigned long rq_saisir(int x, int y, int maxLen,
                                char* buf, bool masquer = false) {
    int pos = 0;
    buf[0] = '\0';
    minitel.echo(false);
    minitel.newXY(x, y);
    minitel.cursor();
    while (true) {
        unsigned long t = minitel.getKeyCode();
        if (t == CORRECTION) {
            if (pos > 0) {
                pos--;
                buf[pos] = '\0';
                minitel.moveCursorLeft(1);
                minitel.print(".");
                minitel.moveCursorLeft(1);
            }
        } else if (t == ENVOI || t == SUITE || t == SOMMAIRE || t == CONNEXION_FIN) {
            minitel.noCursor();
            minitel.echo(true);
            return t;
        } else if (t >= 0x20 && t <= 0x7E && pos < maxLen) {
            buf[pos++] = (char)t;
            buf[pos]   = '\0';
            minitel.print(masquer ? "*" : String((char)t));
        }
    }
}

// ============================================================
// ECRAN ACCUEIL
// ============================================================
static const String RQ_VDT_ACCUEIL =
    "1f,42,45,0e,1b,57,20,12,44,1b,50,50,09,09,09,12,43,09,09,09,09,1b,57,20,20,09,09,09,1b,50,50,09,12,42,09,09,09,09,12,43,"
    "1f,43,45,0e,1b,57,20,20,1b,50,23,23,1b,57,20,20,09,09,1b,40,23,20,20,20,23,09,12,42,20,20,23,09,23,20,23,20,20,23,09,09,23,20,20,20,23,"
    "1f,44,45,0e,1b,57,20,20,1b,50,50,1b,57,20,20,1b,40,50,09,20,20,1b,50,1b,47,50,50,1b,57,20,20,09,1b,40,50,20,20,50,50,09,50,20,20,09,50,50,09,20,20,50,09,20,20,"
    "1f,45,44,0e,1b,57,1b,40,23,20,20,20,20,1b,50,1b,47,23,09,09,1b,57,20,12,42,1b,50,23,12,42,09,09,1b,57,20,20,09,09,09,09,12,42,1b,40,23,09,09,09,20,20,09,09,20,20,"
    "1f,46,44,0e,1b,57,20,20,09,1b,40,50,20,20,1b,50,1b,47,50,09,09,1b,57,1b,40,50,20,20,20,09,09,09,50,20,20,20,09,09,09,20,20,09,09,09,50,20,20,20,50,"
    "1f,47,44,0e,1b,57,20,20,09,09,1b,50,23,1b,57,20,1b,50,23,09,09,09,12,43,09,09,09,09,12,43,09,09,09,12,42,09,09,09,09,12,43,"
    "1f,48,4a,0e,50,50,1b,57,20,20,1b,50,50,09,09,09,12,42,09,09,09,09,09,09,1b,57,20,20,09,09,09,1b,50,50,12,43,"
    "1f,49,49,0e,1b,57,1b,40,23,20,20,1b,50,1b,47,23,23,1b,57,20,1b,40,23,09,09,20,20,09,23,23,09,09,09,1b,50,1b,47,12,42,09,1b,57,1b,40,12,42,20,20,20,20,"
    "1f,4a,49,0e,1b,57,20,20,09,1b,50,50,09,1b,57,20,20,09,12,42,09,09,12,42,09,09,09,12,42,09,1b,40,50,50,1b,50,1b,47,50,1b,57,20,1b,40,50,"
    "1f,4b,49,0e,1b,57,20,20,09,20,1b,40,23,20,1b,50,1b,47,23,09,1b,57,20,20,09,1b,40,23,20,20,09,09,09,20,1b,50,1b,47,23,09,09,1b,57,1b,40,23,20,1b,50,1b,47,23,"
    "1f,4c,49,0e,1b,57,1b,40,50,20,20,20,20,1b,50,1b,47,50,09,09,1b,57,1b,40,50,20,20,20,50,20,20,09,20,20,09,09,20,20,20,20,20,20,"
    "1f,4d,4a,0e,23,12,42,1b,57,20,20,1b,40,23,09,09,1b,50,1b,47,12,43,09,12,42,09,12,42,09,09,12,46,"
    "1f,4e,4e,0e,1b,57,1b,40,50,1f,41,4e,0f,1b,5d,33,36,31,35,1b,5c,"
    "1f,4f,4a,50,72,65,73,73,20,53,74,61,72,74,20,52,65,74,72,6f,67,61,6d,69,6e,67";

static const String RQ_VDT_SELECTION_MODE =
    "1f,42,46,0e,1b,57,1b,40,49,2c,4c,2c,4c,4c,2c,4c,1b,50,1b,47,34,"
    "1f,43,46,0e,1b,57,1b,40,4a,1b,50,1b,47,28,1b,57,1b,40,4a,42,1b,50,1b,47,34,1b,57,1b,40,4a,52,1b,50,1b,47,34,1b,57,1b,40,4a,"
    "1f,44,46,0e,45,1b,57,1b,40,2c,1b,50,1b,47,51,1b,57,1b,40,2c,1b,50,1b,47,51,51,1b,57,1b,40,2c,1b,50,1b,47,50,25,"
    "1f,45,42,0e,48,1b,57,1b,40,23,23,1b,50,1b,47,30,09,50,30,09,48,34,09,30,50,09,09,50,30,09,09,09,09,09,09,30,"
    "1f,46,42,0e,4a,1b,57,1b,40,2a,21,1b,50,1b,47,25,1b,57,1b,40,21,2c,20,1b,50,1b,47,2a,1b,57,20,1b,40,50,1b,50,1b,47,2a,1b,57,20,1b,40,54,1b,50,1b,47,25,1b,57,1b,40,21,58,20,09,09,09,09,09,1b,50,1b,47,30,1b,57,1b,40,4a,1b,50,1b,47,30,"
    "1f,47,42,0e,1b,57,20,1b,40,54,22,1b,50,1b,47,30,1b,57,1b,40,54,22,1b,50,1b,47,34,09,1b,57,1b,40,30,23,09,34,4a,09,30,23,58,09,09,09,09,09,1b,50,1b,47,22,1b,57,1b,40,58,"
    "1f,48,42,0e,23,09,22,50,1b,57,1b,40,23,1b,50,1b,47,30,09,50,09,09,09,1b,57,1b,40,23,09,1b,50,1b,47,40,50,30,"
    "1f,49,44,0e,4a,1b,57,1b,40,48,1b,50,1b,47,30,1b,57,20,1b,50,48,1b,57,1b,40,48,1b,50,1b,47,4a,1b,57,1b,40,4a,09,23,1b,50,1b,47,2a,1b,57,1b,40,38,40,1b,50,1b,47,21,09,09,09,09,09,09,1b,52,1b,40,41,50,1b,50,1b,42,34,1b,47,50,40,30,"
    "1f,4a,44,0e,2a,1b,57,1b,40,22,20,1b,50,1b,47,31,2a,1b,57,1b,40,22,40,22,1b,50,1b,47,48,1b,57,1b,40,4a,1b,50,1b,47,48,1b,57,20,1b,40,23,1b,50,1b,47,34,09,09,09,09,09,09,1b,52,1b,40,40,26,4a,1b,57,42,1b,50,1b,47,4a,4a,"
    "1f,4b,46,0e,22,1b,57,1b,40,58,09,09,09,09,09,09,09,09,09,09,09,09,09,09,09,09,1b,52,32,23,1b,50,1b,42,25,1b,47,23,22,22,"
    "1f,4d,58,0e,1b,52,1b,40,21,30,1b,50,1b,42,34,1b,47,50,09,40,30,40,09,50,09,50,"
    "1f,4e,58,0e,1b,52,1b,40,4a,1b,50,1b,42,25,1b,52,1b,40,4a,1b,57,2a,1b,50,1b,47,25,1b,57,1b,40,4a,09,2a,4a,12,42,42,"
    "1f,4f,58,0e,1b,52,1b,40,32,22,1b,50,1b,42,25,1b,47,21,21,22,21,12,42,23,09,23,"
    "1f,51,58,0e,1b,52,1b,40,21,50,1b,50,1b,42,34,1b,47,30,30,50,09,30,12,42,50,"
    "1f,52,58,0e,1b,52,1b,40,22,44,4a,1b,57,12,42,2a,1b,50,1b,47,25,1b,57,1b,40,4a,12,42,42,"
    "1f,53,58,0e,1b,52,1b,40,32,21,1b,50,1b,42,25,1b,47,22,09,21,21,23,09,21,23,0f,"
    "1f,43,58,53,19,42,65,6c,65,63,74,69,6f,6e,6e,65,"
    "1f,44,58,6c,65,20,6d,6f,64,65,20,64,65,20,6a,65,75";

static void rq_afficheAccueil() {
    rq_totalBanque = rq_chargerQuestions(rq_banque);
    minitel.newScreen();
    checkScreen(RQ_VDT_ACCUEIL, 0, 0);
    // y=18 : JOUER | CLASSEMENTS en 2 colonnes
    minitel.newXY(8, 18);
    minitel.attributs(CARACTERE_VERT);
    minitel.print("  "); minitel.attributs(INVERSION_FOND); minitel.print("J"); minitel.attributs(FOND_NORMAL); minitel.print("OUER");
    minitel.newXY(20, 18);
    minitel.print("  "); minitel.attributs(INVERSION_FOND); minitel.print("C"); minitel.attributs(FOND_NORMAL); minitel.print("LASSEMENTS");
    minitel.attributs(CARACTERE_BLANC);
    // y=23-24 : pied avec ADMIN à gauche, nb questions à droite
    rq_ligneH(23);
    minitel.newXY(1, 24);
    minitel.attributs(CARACTERE_CYAN);
    minitel.print("  "); minitel.attributs(INVERSION_FOND); minitel.print("A"); minitel.attributs(FOND_NORMAL); minitel.print("DMIN");
    char nbq[21];
    sprintf(nbq, "%d questions", rq_totalBanque);
    minitel.newXY(40 - (int)strlen(nbq), 24);
    minitel.print(nbq);
    minitel.attributs(CARACTERE_BLANC);
    minitel.newXY(1, 22);
    minitel.cursor();
    rq_etat = RQ_ACCUEIL;
    currentEcran = "RQ_ACCUEIL";
    Serial.println("RQ: accueil affiche, etat=" + String(rq_etat));
}

// ============================================================
// ADMIN
// ============================================================

static bool rq_adminSaisirMdp() {
    minitel.newScreen();
    rq_bandeau();
    rq_centrer("** ESPACE ADMIN **", 8);
    minitel.newXY(1, 12);
    minitel.print("  Mot de passe : ");
    char buf[9] = "";
    unsigned long t = rq_saisir(19, 12, 8, buf, true);
    if (t == SOMMAIRE || t == CONNEXION_FIN) return false;
    if (strcmp(buf, RQ_MDP_ADMIN) == 0) return true;
    minitel.newXY(1, 15);
    minitel.attributs(CARACTERE_ROUGE);
    minitel.print("  Mot de passe incorrect !      ");
    minitel.attributs(CARACTERE_BLANC);
    delay(1500);
    return false;
}

static void rq_afficheAdminMenu() {
    minitel.newScreen();
    rq_bandeau();
    rq_centrer("** ADMIN **", 3);
    rq_ligneH(5, '=');
    minitel.newXY(1, 8);
    minitel.attributs(CARACTERE_VERT);
    minitel.print("1 | Reinitialiser classements");
    minitel.newXY(1, 10);
    minitel.print("2 | Modifier une question");
    minitel.newXY(1, 12);
    minitel.print("3 | Ajouter une question");
    minitel.attributs(CARACTERE_BLANC);
    minitel.newXY(1, 16);
    minitel.print("Tapez votre choix puis ENVOI");
    rq_pied("SOMMAIRE = accueil");
    rq_etat = RQ_ADMIN_MENU;
    currentEcran = "RQ_ADMIN_MENU";
}

static void rq_afficheAdminReset() {
    minitel.newScreen();
    rq_bandeau();
    rq_centrer("** REINITIALISER CLASSEMENT **", 3);
    rq_ligneH(5, '=');
    minitel.newXY(1, 8);
    minitel.attributs(CARACTERE_VERT);
    minitel.print(""); minitel.attributs(DEBUT_LIGNAGE); minitel.print("Z"); minitel.attributs(FIN_LIGNAGE); minitel.print("EN");
    minitel.newXY(1, 10);
    minitel.print(""); minitel.attributs(DEBUT_LIGNAGE); minitel.print("A"); minitel.attributs(FIN_LIGNAGE); minitel.print("RCADE");
    minitel.newXY(1, 12);
    minitel.print(""); minitel.attributs(DEBUT_LIGNAGE); minitel.print("S"); minitel.attributs(FIN_LIGNAGE); minitel.print("URVIE");
    minitel.newXY(1, 14);
    minitel.print(""); minitel.attributs(DEBUT_LIGNAGE); minitel.print("T"); minitel.attributs(FIN_LIGNAGE); minitel.print("OUS");
    minitel.attributs(CARACTERE_BLANC);
    minitel.newXY(1, 17);
    minitel.print("Tapez votre choix puis ENVOI");
    rq_pied("SOMMAIRE = menu admin");
    rq_etat = RQ_ADMIN_RESET;
    currentEcran = "RQ_ADMIN_RESET";
}

static void rq_afficheAdminListe() {
    rq_totalBanque = rq_chargerQuestions(rq_banque);
    int totalPages = (rq_totalBanque + 4) / 5;
    if (rq_pageAdmin >= totalPages) rq_pageAdmin = 0;
    int debut = rq_pageAdmin * 5;

    minitel.newScreen();
    rq_bandeau();
    rq_centrer("** QUESTIONS **", 3);
    rq_ligneH(4);

    for (int i = 0; i < 5; i++) {
        int idx = debut + i;
        minitel.newXY(1, 5 + i * 2);
        if (idx < rq_totalBanque) {
            String enonce = rq_banque[idx].enonce;
            if ((int)enonce.length() > 33) enonce = enonce.substring(0, 33);
            char ligne[41];
            sprintf(ligne, "  %2d. %s", idx + 1, enonce.c_str());
            minitel.print(ligne);
        }
    }

    char pg[41];
    sprintf(pg, "  Page %d/%d  (%d questions)",
            rq_pageAdmin + 1, (totalPages > 0 ? totalPages : 1), rq_totalBanque);
    minitel.newXY(1, 17); minitel.print(pg);
    minitel.newXY(1, 19); minitel.print("  No question puis ENVOI");
    rq_pied("  SUI/RET=page  SOM=menu admin");
    rq_etat = RQ_ADMIN_LISTE;
    currentEcran = "RQ_ADMIN_LISTE";
}

static void rq_adminEditerQuestion(int idx) {
    const char* labels[6] = {
        "Enonce", "Choix 1 (A)", "Choix 2 (B)",
        "Choix 3 (C)", "Choix 4 (D)", "Bonne rep. (A-D)"
    };
    int maxLens[6] = { 38, 30, 30, 30, 30, 1 };
    char buf[61];

    for (int f = 0; f < 6; f++) {
        minitel.newScreen();
        rq_bandeau();
        rq_centrer("** MODIFIER QUESTION **", 3);
        char header[41];
        sprintf(header, "  Question %d / %d", idx + 1, rq_totalBanque);
        minitel.newXY(1, 5); minitel.print(header);
        minitel.newXY(1, 8);
        minitel.attributs(CARACTERE_CYAN);
        minitel.print("  " + String(labels[f]) + " :");
        minitel.attributs(CARACTERE_BLANC);

        String current;
        if      (f == 0) current = rq_editQ.enonce;
        else if (f <  5) current = rq_editQ.choix[f - 1];
        else             current = String((char)rq_editQ.bonneReponse);
        if ((int)current.length() > 36) current = current.substring(0, 36);
        minitel.newXY(1, 10); minitel.print("  " + current);

        minitel.newXY(1, 14);
        minitel.print("  Nouveau (SUITE = conserver) :");

        memset(buf, 0, sizeof(buf));
        unsigned long t = rq_saisir(3, 16, maxLens[f], buf);
        if (t == CONNEXION_FIN) return;
        if (t == SOMMAIRE)      { rq_afficheAdminListe(); return; }

        if (strlen(buf) > 0) {
            if      (f == 0) rq_editQ.enonce     = String(buf);
            else if (f <  5) rq_editQ.choix[f-1] = String(buf);
            else {
                char rep = toupper((unsigned char)buf[0]);
                if (rep >= 'A' && rep <= 'D') rq_editQ.bonneReponse = rep;
                else { f--; continue; }
            }
        }
    }

    rq_modifierQuestion(idx, rq_editQ);
    minitel.newXY(1, 20);
    minitel.attributs(CARACTERE_VERT);
    minitel.print("  Sauvegarde OK !              ");
    minitel.attributs(CARACTERE_BLANC);
    delay(1200);
    rq_afficheAdminListe();
}

static void rq_adminAjouterQuestion() {
    const char* labels[6] = {
        "Enonce", "Choix 1 (A)", "Choix 2 (B)",
        "Choix 3 (C)", "Choix 4 (D)", "Bonne rep. (A-D)"
    };
    int maxLens[6] = { 38, 30, 30, 30, 30, 1 };
    RQ_Question newQ;
    newQ.bonneReponse = 'A';
    char buf[61];

    for (int f = 0; f < 6; f++) {
        minitel.newScreen();
        rq_bandeau();
        rq_centrer("** AJOUTER QUESTION **", 3);
        minitel.newXY(1, 8);
        minitel.attributs(CARACTERE_CYAN);
        minitel.print("  " + String(labels[f]) + " :");
        minitel.attributs(CARACTERE_BLANC);
        minitel.newXY(1, 12);
        if (f == 5) minitel.print("  Entrez A, B, C ou D :");
        else        minitel.print("  Saisir puis SUITE :");

        memset(buf, 0, sizeof(buf));
        unsigned long t = rq_saisir(3, 14, maxLens[f], buf);
        if (t == CONNEXION_FIN) return;
        if (t == SOMMAIRE)      { rq_afficheAdminMenu(); return; }

        if (f < 5) {
            if (strlen(buf) == 0) { f--; continue; }
            if (f == 0) newQ.enonce     = String(buf);
            else        newQ.choix[f-1] = String(buf);
        } else {
            char rep = toupper((unsigned char)buf[0]);
            if (rep < 'A' || rep > 'D') { f--; continue; }
            newQ.bonneReponse = rep;
        }
    }

    rq_sauvegarderNouvelleQuestion(newQ);
    minitel.newScreen();
    rq_bandeau();
    rq_centrer("Question ajoutee !", 12);
    delay(1500);
    rq_afficheAdminMenu();
}

// ============================================================
// JEU
// ============================================================
static void rq_afficheSelectionMode() {
    minitel.newScreen();
    checkScreen(RQ_VDT_SELECTION_MODE, 0, 0);
    rq_pied("  SOMMAIRE = accueil");
    minitel.newXY(1, 22);
    minitel.cursor();
    rq_etat = RQ_SELECTION_MODE;
    currentEcran = "RQ_SELECTION_MODE";
}

static void rq_demarrerPartie() {
    rq_totalBanque = rq_chargerQuestions(rq_banque);
    Serial.println("RQ: questions chargees=" + String(rq_totalBanque));
    if (rq_mode == RQ_MODE_SURVIE) {
        rq_nbQuestions = rq_totalBanque;
    } else {
        rq_nbQuestions = RQ_NB_QUESTIONS_PARTIE;
        if (rq_totalBanque < RQ_NB_QUESTIONS_PARTIE) {
            minitel.newScreen();
            rq_bandeau();
            minitel.newXY(1, 10);
            minitel.attributs(CARACTERE_ROUGE);
            char msg[41];
            sprintf(msg, "  Besoin de %d questions min (%d dispo).",
                    RQ_NB_QUESTIONS_PARTIE, rq_totalBanque);
            minitel.print(msg);
            minitel.attributs(CARACTERE_BLANC);
            rq_pied("  SOMMAIRE = retour accueil");
            rq_etat = RQ_LEADERBOARD;
            currentEcran = "RQ_ERREUR";
            return;
        }
    }
    rq_tirerQuestions(rq_totalBanque, rq_indices, rq_nbQuestions);
    rq_questionNum = 0;
    rq_score = 0;
    rq_afficherQuestion(0);
    rq_etat = RQ_JEU;
}

static void rq_afficheLeaderboardsNav() {
    minitel.newScreen();
    rq_bandeau();
    minitel.newXY(1, 5);
    minitel.attributs(CARACTERE_CYAN);
    rq_centrer("** CLASSEMENTS **", 5);
    minitel.attributs(CARACTERE_BLANC);
    rq_ligneH(7, '=');
    minitel.newXY(1, 9);
    minitel.attributs(CARACTERE_VERT);
    minitel.print("  "); minitel.attributs(DEBUT_LIGNAGE); minitel.print("Z"); minitel.attributs(FIN_LIGNAGE); minitel.print("EN - classique");
    minitel.newXY(1, 11);
    minitel.print("  "); minitel.attributs(DEBUT_LIGNAGE); minitel.print("A"); minitel.attributs(FIN_LIGNAGE); minitel.print("RCADE - rapidité");
    minitel.attributs(CARACTERE_BLANC);
    minitel.newXY(1, 13);
    minitel.print("  "); minitel.attributs(DEBUT_LIGNAGE); minitel.print("S"); minitel.attributs(FIN_LIGNAGE); minitel.print("URVIE - survie");
    minitel.newXY(1, 16);
    minitel.print("  Tapez Z, A ou M puis ENVOI");
    rq_pied("  SOMMAIRE = accueil");
    rq_etat = RQ_LEADERBOARDS_NAV;
    currentEcran = "RQ_LEADERBOARDS_NAV";
}

static void rq_afficheLeaderboard() {
    RQ_Score lb[RQ_MAX_SCORES];
    rq_chargerLeaderboard(lb, rq_lbMode);

    const char* nomMode = (rq_lbMode == RQ_MODE_ARCADE) ? "ARCADE"
                        : (rq_lbMode == RQ_MODE_SURVIE) ? "SURVIE"
                        : "ZEN";
    int totalPages = (RQ_MAX_SCORES + 9) / 10;
    int debut = rq_lbPage * 10;

    minitel.newScreen();
    rq_bandeau();
    {
        char titre[41];
        sprintf(titre, "** %s **", nomMode);
        int pad = (40 - (int)strlen(titre)) / 2;
        if (pad < 0) pad = 0;
        minitel.newXY(1 + pad, 3);
        minitel.attributs(CARACTERE_JAUNE);
        minitel.print(titre);
        minitel.attributs(CARACTERE_BLANC);
    }
    rq_ligneH(4);
    for (int i = 0; i < 10; i++) {
        int idx = debut + i;
        minitel.newXY(1, 5 + i);
        char ligne[41];
        if (idx < RQ_MAX_SCORES) {
            sprintf(ligne, "  %2d.  %-8s    %4d pts",
                    idx + 1, lb[idx].pseudo, lb[idx].points);
        } else {
            strcpy(ligne, "");
        }
        minitel.print(ligne);
    }
    {
        char pg[41];
        sprintf(pg, "  Page %d / %d", rq_lbPage + 1, totalPages);
        minitel.newXY(1, 16);
        minitel.print(pg);
    }
    minitel.newXY(1, 18);
    minitel.attributs(CARACTERE_CYAN);
    minitel.print("  Z=ZEN  A=ARCADE  M=MS  J=Rejouer");
    if (totalPages > 1) {
        minitel.newXY(1, 20);
        minitel.print("  SUI=page suiv  RET=page prec");
    }
    minitel.attributs(CARACTERE_BLANC);
    rq_pied("  SOMMAIRE = accueil");
    rq_etat = RQ_LEADERBOARD;
    currentEcran = "RQ_LEADERBOARD";
}

// ============================================================
// SETUP
// ============================================================
void setupRetroquiz() {
    Serial.println("RQ: setupRetroquiz START");
    initMinitelService();         // pageMode + newScreen + echo(true)
    currentService = "RETROQUIZ";
    currentEcran   = "RQ_ACCUEIL";
    saisieColor    = CARACTERE_BLANC;
    rq_initialiserFichiers();
    Serial.println("RQ: fichiers initialises");
    rq_afficheAccueil();
    Serial.println("RQ: setupRetroquiz END");
}

// ============================================================
// LOOP — avec debug complet
// ============================================================
void loopRetroquiz() {
    Serial.println("RQ: loopRetroquiz START");
    Serial.println("RQ: touche au demarrage=" + String(touche, HEX));
    Serial.println("RQ: lasttouche=" + String(lasttouche, HEX));
    Serial.println("RQ: userInput=[" + userInput + "]");

    // Vider le buffer serie du Minitel pour ignorer 
    // tout byte residuel du lancement du service
    delay(100);
    while (Serial1.available()) {
        Serial1.read();
        delay(5);
    }
    Serial.println("RQ: buffer vide, entree dans while(1)");

    while (1) {

        // Champ de saisie selon l'etat
        switch (rq_etat) {
            case RQ_ACCUEIL:
                break;
            case RQ_SELECTION_MODE:
                break;
            case RQ_JEU:
                if (rq_mode == RQ_MODE_ARCADE || rq_mode == RQ_MODE_ZEN) {
                    rq_afficherBarreTimer(0, 0, rq_getTimeLimitMs(), true);  // Reset avec paramètre true
                    rq_questionStartTime = millis();  // Chrono démarre immédiatement
                }
                break;
            case RQ_LEADERBOARDS_NAV:
                champVide(12, 16, 1);
                break;
            case RQ_LEADERBOARD:
            case RQ_ADMIN_MENU:
            case RQ_ADMIN_RESET:
                champVide(12, 22, 1);
                break;
            case RQ_ADMIN_LISTE:
                champVide(12, 22, 2);
                break;
        }

        Serial.println("RQ: avant wait_for_user_action, etat=" + String(rq_etat));
        if (rq_etat == RQ_JEU) {
            rq_attendreReponseJeu();
        } else if (rq_etat == RQ_ACCUEIL) {
            rq_attendreAccueil();
        } else if (rq_etat == RQ_SELECTION_MODE) {
            rq_attendreSelectionMode();
        } else {
            wait_for_user_action();
        }

        // DEBUG : afficher ce qu'on a recu
        Serial.println("RQ: apres wait, touche=0x" + String(touche, HEX) 
                       + " userInput=[" + userInput + "]"
                       + " etat=" + String(rq_etat));

        String input = userInput;
        input.trim();
        input.toUpperCase();

        switch (rq_etat) {

            case RQ_ACCUEIL:
                if (touche == CONNEXION_FIN) {
                    Serial.println("RQ: CONNEXION_FIN -> return");
                    return;
                }
                if (touche == 'J' || touche == 'C' || touche == 'A') {
                    Serial.println("RQ: touche=[" + String((char)touche) + "]");
                    if      (touche == 'J') rq_afficheSelectionMode();
                    else if (touche == 'C') rq_afficheLeaderboardsNav();
                    else if (touche == 'A') {
                        if (rq_adminSaisirMdp()) rq_afficheAdminMenu();
                        else rq_afficheAccueil();
                    }
                }
                // SOMMAIRE depuis accueil : on ne quitte PAS
                // (le Minitel pourrait en envoyer un parasite au lancement)
                if (touche == SOMMAIRE) {
                    Serial.println("RQ: SOMMAIRE recu sur ACCUEIL, ignore");
                    // Ne pas retourner — redemander un input
                }
                break;

            case RQ_SELECTION_MODE:
                if (touche == CONNEXION_FIN) return;
                if (touche == SOMMAIRE || touche == RETOUR) { rq_afficheAccueil(); break; }
                if (touche == 'Z' || touche == 'A' || touche == 'S') {
                    if      (touche == 'Z') { rq_mode = RQ_MODE_ZEN;    rq_demarrerPartie(); }
                    else if (touche == 'A') { rq_mode = RQ_MODE_ARCADE; rq_demarrerPartie(); }
                    else if (touche == 'S') { rq_mode = RQ_MODE_SURVIE; rq_demarrerPartie(); }
                }
                break;

            case RQ_JEU:
                if (touche == CONNEXION_FIN) { Serial.println("RQ: CF->return"); return; }
                if (touche == SOMMAIRE) { rq_afficheAccueil(); break; }
                if (touche == RETOUR) { rq_afficheSelectionMode(); break; }
                {
                    bool valide  = false;
                    bool correct = false;
                    if (rq_timedOut) {
                        minitel.newXY(1, 21);
                        minitel.attributs(CARACTERE_ROUGE);
                        minitel.print("  TEMPS ECOULE !  (0 pt)            ");
                        minitel.attributs(CARACTERE_BLANC);
                        minitel.bip();
                        delay(200);
                        minitel.bip();
                        delay(1200);
                        valide = true;
                    } else if (touche == ENVOI || touche == SUITE) {
                        if (input == "1" || input == "2" || input == "3" || input == "4") {
                            rq_reponse = 'A' + (input.charAt(0) - '1');
                            RQ_Question& q = rq_banque[rq_indices[rq_questionNum]];
                            correct = (rq_reponse == q.bonneReponse);
                            int pts = 0;
                            if (rq_mode == RQ_MODE_SURVIE) {
                                if (correct) pts = 1;
                            } else {
                                unsigned long elapsed  = rq_responseTime - rq_questionStartTime;
                                unsigned long limitMs  = rq_getTimeLimitMs();
                                unsigned long diviseur = (rq_mode == RQ_MODE_ZEN) ? 200UL : 100UL;
                                if (correct && elapsed < limitMs) {
                                    pts = (int)((limitMs - elapsed) / diviseur);
                                    if (pts > 100) pts = 100;  // Cap à 100 points
                                }
                            }
                            minitel.newXY(1, 21);
                            if (correct) {
                                rq_score += pts;
                                minitel.attributs(CARACTERE_VERT);
                                char msg[41];
                                if (rq_mode == RQ_MODE_SURVIE)
                                    sprintf(msg, "  BONNE REPONSE !                   ");
                                else
                                    sprintf(msg, "  BONNE REPONSE !  +%4d pts         ", pts);
                                minitel.print(msg);
                                minitel.attributs(CARACTERE_BLANC);
                                minitel.bip();
                            } else {
                                // Animation uniquement pour ZEN et ARCADE
                                if (rq_mode == RQ_MODE_ZEN || rq_mode == RQ_MODE_ARCADE) {
                                    // Retirer l'inversion de la réponse utilisateur
                                    rq_highlightReponse(-1);

                                    // Faire clignoter la bonne réponse 3 fois
                                    int bonneIdx = q.bonneReponse - 'A';
                                    for (int i = 0; i < 3; i++) {
                                        rq_highlightReponse(bonneIdx);
                                        delay(250);
                                        rq_highlightReponse(-1);
                                        delay(250);
                                    }

                                    // Afficher le message
                                    minitel.newXY(1, 21);
                                    minitel.attributs(CARACTERE_ROUGE);
                                    minitel.print("  MAUVAISE REPONSE                    ");
                                    minitel.attributs(CARACTERE_BLANC);
                                } else {
                                    // Mode SURVIE : comportement simplifié
                                    minitel.newXY(1, 21);
                                    minitel.attributs(CARACTERE_ROUGE);
                                    minitel.print("  MAUVAISE REPONSE                    ");
                                    minitel.attributs(CARACTERE_BLANC);
                                }

                                // Double bip pour tous les modes
                                minitel.bip();
                                delay(200);
                                minitel.bip();
                            }
                            delay(3000);
                            valide = true;
                        }
                    }
                    if (valide) {
                        rq_questionNum++;
                        if (rq_mode == RQ_MODE_SURVIE && !correct)
                            rq_questionNum = rq_nbQuestions;
                        if (rq_questionNum >= rq_nbQuestions) {
                            minitel.newScreen();
                            rq_bandeau();
                            char sc[40];
                            if (rq_mode == RQ_MODE_ZEN)
                                sprintf(sc, "Score : %d / %d pts", rq_score, 100 * rq_nbQuestions);
                            else if (rq_mode == RQ_MODE_SURVIE)
                                sprintf(sc, "Score : %d bonnes reponses", rq_score);
                            else
                                sprintf(sc, "Score : %d / %d pts", rq_score,
                                        100 * rq_nbQuestions);
                            rq_centrer(String(sc), 10);
                            minitel.newXY(1, 13);
                            minitel.print("  Pseudo (SUITE pour valider) :");
                            minitel.newXY(1, 15); minitel.print("  > [");
                            minitel.newXY(35, 15); minitel.print("]");
                            rq_pied("  Saisissez votre pseudo puis SUITE");
                            memset(rq_pseudo, 0, sizeof(rq_pseudo));
                            rq_saisir(5, 15, RQ_MAX_PSEUDO, rq_pseudo);
                            if (strlen(rq_pseudo) == 0) strcpy(rq_pseudo, "ANONYME");
                            RQ_Score lb[RQ_MAX_SCORES];
                            rq_chargerLeaderboard(lb, rq_mode);
                            rq_insererScore(lb, rq_pseudo, rq_score, rq_mode);
                            rq_lbMode = rq_mode;
                            rq_lbPage = 0;
                            rq_afficheLeaderboard();
                        } else {
                            rq_afficherQuestion(rq_questionNum);
                        }
                    }
                }
                break;

            case RQ_LEADERBOARDS_NAV:
                if (touche == CONNEXION_FIN) return;
                if (touche == SOMMAIRE) { rq_afficheAccueil(); break; }
                if (touche == ENVOI || touche == SUITE) {
                    if      (input == "Z") { rq_lbMode = RQ_MODE_ZEN;         rq_lbPage = 0; rq_afficheLeaderboard(); }
                    else if (input == "A") { rq_lbMode = RQ_MODE_ARCADE;      rq_lbPage = 0; rq_afficheLeaderboard(); }
                    else if (input == "S") { rq_lbMode = RQ_MODE_SURVIE; rq_lbPage = 0; rq_afficheLeaderboard(); }
                }
                break;

            case RQ_LEADERBOARD:
                if (touche == CONNEXION_FIN) return;
                if (touche == SOMMAIRE) { rq_afficheAccueil(); break; }
                if (touche == SUITE) {
                    if (rq_lbPage < (RQ_MAX_SCORES + 9) / 10 - 1) {
                        rq_lbPage++;
                        rq_afficheLeaderboard();
                    }
                    break;
                }
                if (touche == RETOUR) {
                    if (rq_lbPage > 0) {
                        rq_lbPage--;
                        rq_afficheLeaderboard();
                    }
                    break;
                }
                if (touche == ENVOI) {
                    if      (input == "Z") { rq_lbMode = RQ_MODE_ZEN;         rq_lbPage = 0; rq_afficheLeaderboard(); }
                    else if (input == "A") { rq_lbMode = RQ_MODE_ARCADE;      rq_lbPage = 0; rq_afficheLeaderboard(); }
                    else if (input == "S") { rq_lbMode = RQ_MODE_SURVIE; rq_lbPage = 0; rq_afficheLeaderboard(); }
                    else if (input == "J") { rq_demarrerPartie(); }
                }
                break;

            case RQ_ADMIN_MENU:
                if (touche == CONNEXION_FIN) return;
                if (touche == SOMMAIRE) { rq_afficheAccueil(); break; }
                if (touche == ENVOI || touche == SUITE) {
                    if      (input == "1") rq_afficheAdminReset();
                    else if (input == "2") { rq_pageAdmin = 0; rq_afficheAdminListe(); }
                    else if (input == "3") rq_adminAjouterQuestion();
                }
                break;

            case RQ_ADMIN_RESET:
                if (touche == CONNEXION_FIN) return;
                if (touche == SOMMAIRE) { rq_afficheAdminMenu(); break; }
                if (touche == ENVOI || touche == SUITE) {
                    int modeReset = -1;
                    if      (input == "Z") modeReset = RQ_MODE_ZEN;
                    else if (input == "A") modeReset = RQ_MODE_ARCADE;
                    else if (input == "S") modeReset = RQ_MODE_SURVIE;
                    if (modeReset >= 0) {
                        rq_reinitialiserLeaderboard(modeReset);
                    } else if (input == "T") {
                        for (int m = 0; m < 3; m++) rq_reinitialiserLeaderboard(m);
                        modeReset = 0;
                    }
                    if (modeReset >= 0) {
                        minitel.newXY(1, 20);
                        minitel.attributs(CARACTERE_VERT);
                        minitel.print("  Classement(s) reinitialise(s) ");
                        minitel.attributs(CARACTERE_BLANC);
                        delay(1200);
                        rq_afficheAdminMenu();
                    }
                }
                break;

            case RQ_ADMIN_LISTE:
                if (touche == CONNEXION_FIN) return;
                if (touche == SOMMAIRE) { rq_afficheAdminMenu(); break; }
                if (touche == SUITE) {
                    int totalP = (rq_totalBanque + 4) / 5;
                    if (rq_pageAdmin < totalP - 1) { rq_pageAdmin++; rq_afficheAdminListe(); }
                    break;
                }
                if (touche == RETOUR) {
                    if (rq_pageAdmin > 0) { rq_pageAdmin--; rq_afficheAdminListe(); }
                    break;
                }
                if (touche == ENVOI) {
                    int n = input.toInt();
                    if (n >= 1 && n <= rq_totalBanque) {
                        rq_adminEditIdx = n - 1;
                        rq_editQ = rq_banque[rq_adminEditIdx];
                        rq_adminEditerQuestion(rq_adminEditIdx);
                    }
                }
                break;

            default:
                if (touche == CONNEXION_FIN) return;
                break;
        }
    }
}
