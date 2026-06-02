// ============================================================
//  RETROQUIZ.INO — VERSION DEBUG
//  Affiche dans le Moniteur Serie ce qui se passe exactement.
//  Ouvrir le moniteur serie a 115200 bauds pendant le test.
// ============================================================

#include "donnees_rq.h"

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
#define RQ_MODE_MORT_SUBITE 2

#define RQ_TEMPS_LIMITE_ZEN_MS 20000

static unsigned long rq_getTimeLimitMs() {
    return (rq_mode == RQ_MODE_ZEN)
           ? (unsigned long)RQ_TEMPS_LIMITE_ZEN_MS
           : (unsigned long)RQ_TEMPS_LIMITE_MS;
}

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
        if (idxL == idx) minitel.attributs(INVERSION_FOND);
        minitel.print(b1L);
        minitel.attributs(FOND_NOIR); minitel.attributs(CARACTERE_BLANC);
        minitel.newXY(21, y);
        if (idxR == idx) minitel.attributs(INVERSION_FOND);
        minitel.print(b1R);
        minitel.attributs(FOND_NOIR); minitel.attributs(CARACTERE_BLANC);

        minitel.newXY(1, y+1);
        if (idxL == idx) minitel.attributs(INVERSION_FOND);
        minitel.print(b2L);
        minitel.attributs(FOND_NOIR); minitel.attributs(CARACTERE_BLANC);
        minitel.newXY(21, y+1);
        if (idxR == idx) minitel.attributs(INVERSION_FOND);
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
    minitel.newXY(1, 19);
    if (rq_mode == RQ_MODE_ARCADE)
        minitel.print("  Appuyez sur 1, 2, 3 ou 4          ");
    else
        minitel.print("  Tapez 1, 2, 3 ou 4 puis ENVOI     ");
    rq_pied("  1/2/3/4 + ENVOI  SOM=Accueil");
    rq_reponse = 0;
    currentEcran = "RQ_JEU";
}

static void rq_afficherBarreTimer(unsigned long elapsed, int barWidth, unsigned long limitMs) {
    int couleurFond;
    unsigned long remaining = (elapsed < limitMs) ? limitMs - elapsed : 0;
    if      (remaining > limitMs * 7 / 10) couleurFond = FOND_VERT;
    else if (remaining > limitMs * 3 / 10) couleurFond = FOND_JAUNE;
    else                                    couleurFond = FOND_ROUGE;

    minitel.noCursor();
    minitel.newXY(3, 21);
    minitel.attributs(FOND_NOIR);
    for (int i = 0; i < 36 - barWidth; i++) minitel.print(" ");
    minitel.attributs(couleurFond);
    for (int i = 0; i < barWidth; i++) minitel.print(" ");
    minitel.attributs(FOND_NOIR);
    minitel.attributs(CARACTERE_BLANC);
    minitel.newXY(12, 20);
    minitel.cursor();
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
            int barWidth = (int)(36L * (long)remaining / limitMs);
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
        unsigned long k = (rq_mode == RQ_MODE_MORT_SUBITE)
                          ? minitel.getKeyCode()
                          : minitel.getKeyCode(false);
        if (k == 0) continue;
        if (k == CONNEXION_FIN || k == SOMMAIRE || k == SUITE || k == ENVOI) {
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
            minitel.newXY(12, 20);
            minitel.print(String(ch));
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
static void rq_afficheAccueil() {
    minitel.newScreen();
    rq_bandeau();
    minitel.newXY(1, 5);
    minitel.attributs(CARACTERE_CYAN);
    minitel.print("  ** 3615 RETROQUIZ **");
    minitel.attributs(CARACTERE_BLANC);
    minitel.newXY(1, 7);
    minitel.print("  Le quiz du retrogaming !");
    rq_ligneH(9, '=');
    minitel.newXY(1, 12);
    minitel.attributs(CARACTERE_VERT);
    minitel.print("  [ 1 ]  JOUER AU QUIZ");
    minitel.newXY(1, 14);
    minitel.print("  [ 2 ]  LEADERBOARDS");
    minitel.attributs(CARACTERE_BLANC);
    minitel.newXY(1, 17);
    minitel.print("  Tapez votre choix puis ENVOI");
    rq_pied("  CONNEXION/FIN pour quitter");
    rq_etat = RQ_ACCUEIL;
    currentEcran = "RQ_ACCUEIL";
    Serial.println("RQ: accueil affiche, etat=" + String(rq_etat));
}

// ============================================================
// JEU
// ============================================================
static void rq_afficheSelectionMode() {
    minitel.newScreen();
    rq_bandeau();
    minitel.newXY(1, 5);
    minitel.attributs(CARACTERE_CYAN);
    rq_centrer("** CHOISISSEZ UN MODE **", 5);
    minitel.attributs(CARACTERE_BLANC);
    rq_ligneH(7, '=');
    minitel.newXY(1, 9);
    minitel.attributs(CARACTERE_VERT);
    minitel.print("  [ 1 ]  ZEN       - Quiz classique");
    minitel.newXY(1, 11);
    minitel.print("  [ 2 ]  ARCADE    - 10s par question !");
    minitel.attributs(CARACTERE_BLANC);
    minitel.newXY(1, 13);
    minitel.print("  [ 3 ]  MORT SUBITE - bientot dispo");
    minitel.newXY(1, 16);
    minitel.print("  Tapez votre choix puis ENVOI");
    rq_pied("  SOMMAIRE = accueil");
    rq_etat = RQ_SELECTION_MODE;
    currentEcran = "RQ_SELECTION_MODE";
}

static void rq_demarrerPartie() {
    rq_totalBanque = rq_chargerQuestions(rq_banque);
    Serial.println("RQ: questions chargees=" + String(rq_totalBanque));
    if (rq_mode == RQ_MODE_MORT_SUBITE) {
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
    rq_centrer("** LEADERBOARDS **", 5);
    minitel.attributs(CARACTERE_BLANC);
    rq_ligneH(7, '=');
    minitel.newXY(1, 9);
    minitel.attributs(CARACTERE_VERT);
    minitel.print("  [ Z ]  ZEN       - scores classiques");
    minitel.newXY(1, 11);
    minitel.print("  [ A ]  ARCADE    - scores par rapidite");
    minitel.attributs(CARACTERE_BLANC);
    minitel.newXY(1, 13);
    minitel.print("  [ M ]  MORT SUBITE - bientot disponible");
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
                        : (rq_lbMode == RQ_MODE_MORT_SUBITE) ? "MORT SUBITE"
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
    minitel.print("  Z=ZEN  A=ARCADE  M=MS (+ENVOI)");
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
                champVide(12, 17, 1);
                break;
            case RQ_SELECTION_MODE:
                champVide(12, 16, 1);
                break;
            case RQ_JEU:
                champVide(12, 20, 1);
                if (rq_mode == RQ_MODE_ARCADE || rq_mode == RQ_MODE_ZEN) {
                    delay(rq_mode == RQ_MODE_ARCADE ? 1000 : 2000);
                    rq_questionStartTime = millis();
                    rq_afficherBarreTimer(0, 36, rq_getTimeLimitMs());
                }
                break;
            case RQ_LEADERBOARDS_NAV:
                champVide(12, 16, 1);
                break;
            case RQ_LEADERBOARD:
            case RQ_ADMIN_MENU:
            case RQ_ADMIN_RESET:
            case RQ_ADMIN_LISTE:
                champVide(12, 22, 1);
                break;
        }

        Serial.println("RQ: avant wait_for_user_action, etat=" + String(rq_etat));
        if (rq_etat == RQ_JEU) {
            rq_attendreReponseJeu();
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
                if (touche == ENVOI || touche == SUITE) {
                    Serial.println("RQ: ENVOI/SUITE, input=[" + input + "]");
                    if      (input == "1") rq_afficheSelectionMode();
                    else if (input == "2") rq_afficheLeaderboardsNav();
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
                if (touche == SOMMAIRE) { rq_afficheAccueil(); break; }
                if (touche == ENVOI || touche == SUITE) {
                    if      (input == "1") { rq_mode = RQ_MODE_ZEN;         rq_demarrerPartie(); }
                    else if (input == "2") { rq_mode = RQ_MODE_ARCADE;      rq_demarrerPartie(); }
                    else if (input == "3") { rq_mode = RQ_MODE_MORT_SUBITE; rq_demarrerPartie(); }
                }
                break;

            case RQ_JEU:
                if (touche == CONNEXION_FIN) { Serial.println("RQ: CF->return"); return; }
                if (touche == SOMMAIRE) { rq_afficheAccueil(); break; }
                {
                    bool valide  = false;
                    bool correct = false;
                    if (rq_timedOut) {
                        minitel.newXY(1, 21);
                        minitel.attributs(CARACTERE_ROUGE);
                        minitel.print("  TEMPS ECOULE !  (0 pt)            ");
                        minitel.attributs(CARACTERE_BLANC);
                        minitel.bip();
                        delay(1200);
                        valide = true;
                    } else if (touche == ENVOI || touche == SUITE) {
                        if (input == "1" || input == "2" || input == "3" || input == "4") {
                            rq_reponse = 'A' + (input.charAt(0) - '1');
                            RQ_Question& q = rq_banque[rq_indices[rq_questionNum]];
                            correct = (rq_reponse == q.bonneReponse);
                            int pts = 0;
                            if (rq_mode == RQ_MODE_MORT_SUBITE) {
                                if (correct) pts = 1;
                            } else {
                                unsigned long elapsed  = millis() - rq_questionStartTime;
                                unsigned long limitMs  = rq_getTimeLimitMs();
                                unsigned long diviseur = (rq_mode == RQ_MODE_ZEN) ? 200UL : 100UL;
                                if (correct && elapsed < limitMs)
                                    pts = (int)((limitMs - elapsed) / diviseur);
                            }
                            minitel.newXY(1, 21);
                            if (correct) {
                                rq_score += pts;
                                minitel.attributs(CARACTERE_VERT);
                                char msg[41];
                                if (rq_mode == RQ_MODE_MORT_SUBITE)
                                    sprintf(msg, "  BONNE REPONSE !                   ");
                                else
                                    sprintf(msg, "  BONNE REPONSE !  +%4d pts         ", pts);
                                minitel.print(msg);
                            } else {
                                minitel.attributs(CARACTERE_ROUGE);
                                char msg[41];
                                sprintf(msg, "  FAUX ! Rep : %c   (0 pt)           ", '1' + (q.bonneReponse - 'A'));
                                minitel.print(msg);
                            }
                            minitel.attributs(CARACTERE_BLANC);
                            minitel.bip();
                            delay(1200);
                            valide = true;
                        }
                    }
                    if (valide) {
                        rq_questionNum++;
                        if (rq_mode == RQ_MODE_MORT_SUBITE && !correct)
                            rq_questionNum = rq_nbQuestions;
                        if (rq_questionNum >= rq_nbQuestions) {
                            minitel.newScreen();
                            rq_bandeau();
                            char sc[40];
                            if (rq_mode == RQ_MODE_ZEN)
                                sprintf(sc, "Score : %d / %d pts", rq_score, 100 * rq_nbQuestions);
                            else if (rq_mode == RQ_MODE_MORT_SUBITE)
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
                    else if (input == "M") { rq_lbMode = RQ_MODE_MORT_SUBITE; rq_lbPage = 0; rq_afficheLeaderboard(); }
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
                    else if (input == "M") { rq_lbMode = RQ_MODE_MORT_SUBITE; rq_lbPage = 0; rq_afficheLeaderboard(); }
                }
                break;

            default:
                if (touche == CONNEXION_FIN) return;
                break;
        }
    }
}
