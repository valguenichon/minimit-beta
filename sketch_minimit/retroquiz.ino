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
#define RQ_SELECTION_MODE 6
#define RQ_MAX_BANQUE 100

#define RQ_MODE_ZEN         0
#define RQ_MODE_ARCADE      1
#define RQ_MODE_MORT_SUBITE 2

static int rq_etat = RQ_ACCUEIL;

static RQ_Question rq_banque[RQ_MAX_BANQUE];
static int         rq_totalBanque = 0;
static int         rq_indices[RQ_NB_QUESTIONS_PARTIE];
static int         rq_questionNum = 0;
static int         rq_score       = 0;
static char        rq_reponse     = 0;
static char        rq_pseudo[RQ_MAX_PSEUDO + 1] = "";
static int           rq_pageAdmin         = 0;
static RQ_Question   rq_editQ;
static unsigned long rq_questionStartTime  = 0;
static bool          rq_timedOut           = false;
static int           rq_mode               = RQ_MODE_ZEN;

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
                numQ, RQ_NB_QUESTIONS_PARTIE);
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

static void rq_afficherQuestion(int numQ) {
    RQ_Question& q = rq_banque[rq_indices[numQ]];
    minitel.newScreen();
    rq_bandeau(numQ + 1);
    minitel.newXY(1, 3);
    minitel.attributs(CARACTERE_CYAN);
    minitel.print("  " + q.enonce);
    minitel.attributs(CARACTERE_BLANC);
    rq_ligneH(7);
    const char lettres[] = {'A','B','C','D'};
    for (int i = 0; i < 4; i++) {
        minitel.newXY(1, 9 + i * 2);
        char label[5];
        sprintf(label, "  %c/ ", lettres[i]);
        minitel.print(label);
        minitel.print(q.choix[i]);
    }
    minitel.newXY(1, 19);
    if (rq_mode == RQ_MODE_ARCADE)
        minitel.print("  Appuyez sur A, B, C ou D          ");
    else
        minitel.print("  Tapez A, B, C ou D puis ENVOI     ");
    rq_pied("  A/B/C/D + ENVOI  SOM=Accueil");
    rq_reponse = 0;
    currentEcran = "RQ_JEU";
}

static void rq_afficherTimerRestant(int sec) {
    minitel.noCursor();
    minitel.newXY(1, 21);
    minitel.attributs(sec <= 3 ? CARACTERE_ROUGE : CARACTERE_JAUNE);
    char buf[41];
    sprintf(buf, "  Temps restant : %2ds               ", sec);
    minitel.print(buf);
    minitel.attributs(CARACTERE_BLANC);
    minitel.newXY(12, 20);
    minitel.cursor();
}

static void rq_attendreReponseAvecTimer() {
    rq_timedOut = false;
    touche = 0;
    userInput = "";
    minitel.echo(false);
    int dernierSec = -1;
    while (true) {
        unsigned long elapsed = millis() - rq_questionStartTime;
        int sec = (int)((RQ_TEMPS_LIMITE_MS - (long)elapsed) / 1000);
        if (sec < 0) sec = 0;
        if (sec != dernierSec) {
            dernierSec = sec;
            rq_afficherTimerRestant(sec);
        }
        if (elapsed >= (unsigned long)RQ_TEMPS_LIMITE_MS) {
            rq_timedOut = true;
            minitel.noCursor();
            minitel.echo(true);
            return;
        }
        unsigned long k = minitel.getKeyCode(false);
        if (k == 0) continue;
        if (k == CONNEXION_FIN || k == SOMMAIRE || k == SUITE || k == ENVOI) {
            touche = k;
            lasttouche = k;
            minitel.noCursor();
            minitel.echo(true);
            return;
        } else if (k == CORRECTION) {
            if (userInput.length() > 0) {
                userInput.remove(userInput.length() - 1);
                minitel.moveCursorLeft(1);
                minitel.print(".");
                minitel.moveCursorLeft(1);
            }
        } else if (k >= 0x20 && k <= 0x7E && userInput.length() == 0) {
            char c = toupper((char)k);
            userInput += c;
            minitel.print(String(c));
            lasttouche = k;
            touche = k;
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
    minitel.print("  [ 2 ]  LEADERBOARD");
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
    rq_tirerQuestions(rq_totalBanque, rq_indices);
    rq_questionNum = 0;
    rq_score = 0;
    rq_afficherQuestion(0);
    rq_etat = RQ_JEU;
}

static void rq_afficheLeaderboard() {
    RQ_Score lb[RQ_MAX_SCORES];
    rq_chargerLeaderboard(lb);
    minitel.newScreen();
    rq_bandeau();
    minitel.newXY(1, 3);
    minitel.attributs(CARACTERE_JAUNE);
    rq_centrer("** TOP 10 **", 3);
    minitel.attributs(CARACTERE_BLANC);
    for (int i = 0; i < RQ_MAX_SCORES; i++) {
        minitel.newXY(1, 5 + i);
        char ligne[41];
        sprintf(ligne, "   %2d.  %-8s    %4d pts",
                i + 1, lb[i].pseudo, lb[i].points);
        minitel.print(ligne);
    }
    rq_pied("  SOMMAIRE ou CONNEXION/FIN = quitter");
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
                if (rq_mode == RQ_MODE_ARCADE) {
                    rq_questionStartTime = millis();
                    rq_afficherTimerRestant(RQ_TEMPS_LIMITE_MS / 1000);
                }
                break;
            case RQ_LEADERBOARD:
            case RQ_ADMIN_MENU:
            case RQ_ADMIN_RESET:
            case RQ_ADMIN_LISTE:
                champVide(12, 22, 1);
                break;
        }

        Serial.println("RQ: avant wait_for_user_action, etat=" + String(rq_etat));
        if (rq_etat == RQ_JEU && rq_mode == RQ_MODE_ARCADE) {
            rq_attendreReponseAvecTimer();
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
                    else if (input == "2") rq_afficheLeaderboard();
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
                    else if (input == "3") {
                        minitel.newXY(1, 19);
                        minitel.attributs(CARACTERE_ROUGE);
                        minitel.print("  BIENTOT DISPONIBLE !              ");
                        minitel.attributs(CARACTERE_BLANC);
                    }
                }
                break;

            case RQ_JEU:
                if (touche == CONNEXION_FIN) { Serial.println("RQ: CF->return"); return; }
                if (touche == SOMMAIRE) { rq_afficheAccueil(); break; }
                {
                    bool valide = false;
                    if (rq_timedOut) {
                        minitel.newXY(1, 21);
                        minitel.attributs(CARACTERE_ROUGE);
                        minitel.print("  TEMPS ECOULE !  (0 pt)            ");
                        minitel.attributs(CARACTERE_BLANC);
                        minitel.bip();
                        delay(1200);
                        valide = true;
                    } else if (touche == ENVOI || touche == SUITE) {
                        if (input == "A" || input == "B" || input == "C" || input == "D") {
                            rq_reponse = input.charAt(0);
                            RQ_Question& q = rq_banque[rq_indices[rq_questionNum]];
                            bool correct = (rq_reponse == q.bonneReponse);
                            int pts = 0;
                            if (rq_mode == RQ_MODE_ZEN) {
                                if (correct) pts = 1;
                            } else {
                                unsigned long elapsed = millis() - rq_questionStartTime;
                                if (correct && elapsed < (unsigned long)RQ_TEMPS_LIMITE_MS)
                                    pts = (int)((RQ_TEMPS_LIMITE_MS - elapsed) / 100);
                            }
                            minitel.newXY(1, 21);
                            if (correct) {
                                rq_score += pts;
                                minitel.attributs(CARACTERE_VERT);
                                char msg[41];
                                if (rq_mode == RQ_MODE_ZEN)
                                    sprintf(msg, "  BONNE REPONSE !                   ");
                                else
                                    sprintf(msg, "  BONNE REPONSE !  +%3d pts          ", pts);
                                minitel.print(msg);
                            } else {
                                minitel.attributs(CARACTERE_ROUGE);
                                char msg[41];
                                sprintf(msg, "  FAUX ! Rep : %c   (0 pt)           ", q.bonneReponse);
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
                        if (rq_questionNum >= RQ_NB_QUESTIONS_PARTIE) {
                            minitel.newScreen();
                            rq_bandeau();
                            char sc[40];
                            if (rq_mode == RQ_MODE_ZEN)
                                sprintf(sc, "Score : %d / %d", rq_score, RQ_NB_QUESTIONS_PARTIE);
                            else
                                sprintf(sc, "Score : %d / %d pts", rq_score,
                                        (RQ_TEMPS_LIMITE_MS / 100) * RQ_NB_QUESTIONS_PARTIE);
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
                            rq_chargerLeaderboard(lb);
                            rq_insererScore(lb, rq_pseudo, rq_score);
                            rq_afficheLeaderboard();
                        } else {
                            rq_afficherQuestion(rq_questionNum);
                        }
                    }
                }
                break;

            case RQ_LEADERBOARD:
                if (touche == CONNEXION_FIN) { Serial.println("RQ: CF->return"); return; }
                if (touche == SOMMAIRE || touche == ENVOI || touche == SUITE) {
                    rq_afficheAccueil();
                }
                break;

            default:
                if (touche == CONNEXION_FIN) return;
                break;
        }
    }
}
