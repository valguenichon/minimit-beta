// ============================================================
//  DONNEES_RQ.CPP
//  Stockage : LittleFS pour les questions, Preferences/NVS pour les scores
//
//  Questions : /rq_questions.txt dans LittleFS
//  Format attendu :
//    Enonce|Choix A|Choix B|Choix C|Choix D|Bonne reponse
//
//  Exemple :
//    En quelle annee fut lance le Minitel ?|1978|1982|1989|1995|B
//
//  Scores : Preferences/NVS, namespace "rq"
// ============================================================

#include "rq_donnees.h"

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <string.h>
#include <esp_system.h>

// ============================================================
// CONFIGURATION
// ============================================================
static const char* RQ_FICHIER_QUESTIONS = "/rq_questions.txt";
static const char* RQ_NAMESPACE_NVS     = "rq";

// ============================================================
// QUESTIONS PAR DEFAUT
// Utilisees uniquement si /rq_questions.txt est absent ou invalide.
// Elles ne sont plus stockees dans NVS.
// ============================================================
static const char* RQ_DEFAUT[][6] = {
    {"En quelle annee fut lance le Minitel ?",
     "1978","1982","1989","1995","B"},
    {"Quel numero pour acceder aux services ?",
     "3611","3614","3615","3617","C"},
    {"Largeur de l'ecran Minitel en colonnes ?",
     "20","40","80","120","B"},
    {"Quel composant est au coeur du Minimit ?",
     "Raspberry Pi","Arduino Uno","ESP32","Atmel AVR","C"},
    {"Hauteur de l'ecran Minitel en lignes ?",
     "12","18","24","32","C"},
    {"Quel protocole utilise le Minitel ?",
     "HTML","Videotex","FTP","Telnet","B"},
    {"En quelle annee le Minitel fut arrete ?",
     "2005","2009","2012","2017","C"},
    {"Qui a deploye le Minitel en France ?",
     "L ORTF","France Telecom","Minitel Club","Thomson","B"},
    {"A quelle vitesse le Minimit communique ?",
     "1200 bauds","2400 bauds","4800 bauds","9600 bauds","A"},
    {"Que signifie ESP dans ESP32 ?",
     "Espressif","Extra Speed","Easy Serial","Echo Serial","A"},
};
static const int RQ_NB_DEFAUT = 10;

// ============================================================
// UTILITAIRES INTERNES
// ============================================================

static bool rq_monterLittleFS() {
    // false = ne formate pas automatiquement la partition.
    // Cela evite d'effacer /rq_questions.txt en cas de probleme de montage.
    static bool dejaMonte = false;
    if (dejaMonte) return true;

    if (!LittleFS.begin(false)) {
        Serial.println("RQ: ERREUR LittleFS.begin() impossible");
        return false;
    }

    dejaMonte = true;
    return true;
}

static String rq_cleScore(int num, const char* suffix) {
    return "s" + String(num) + "_" + suffix;
}

static void rq_copierPseudo(RQ_Score& score, const char* pseudo) {
    const size_t capacite = sizeof(score.pseudo);
    if (capacite == 0) return;

    if (pseudo == nullptr) pseudo = "---";
    strncpy(score.pseudo, pseudo, capacite - 1);
    score.pseudo[capacite - 1] = '\0';
}

// Anciennes cles NVS du fichier precedent, utilisees seulement pour migration.
static String rq_cleQuestionNVS(int num, const char* suffix) {
    return "q" + String(num) + "_" + suffix;
}

static char rq_normaliserReponse(char r) {
    if (r >= 'a' && r <= 'd') r = r - 'a' + 'A';
    if (r < 'A' || r > 'D') return 'A';
    return r;
}

static String rq_nettoyerChampPourFichier(String valeur) {
    valeur.replace("\r", " ");
    valeur.replace("\n", " ");
    valeur.replace("|", "/");
    valeur.trim();
    return valeur;
}

static bool rq_parserLigneQuestion(String ligne, RQ_Question& q) {
    ligne.trim();

    // Ignore BOM UTF-8 eventuel en debut de fichier.
    if (ligne.length() >= 3 &&
        (uint8_t)ligne[0] == 0xEF &&
        (uint8_t)ligne[1] == 0xBB &&
        (uint8_t)ligne[2] == 0xBF) {
        ligne.remove(0, 3);
        ligne.trim();
    }

    if (ligne.length() == 0) return false;
    if (ligne.startsWith("#")) return false;

    String champs[6];
    int debut = 0;

    for (int i = 0; i < 5; i++) {
        int sep = ligne.indexOf('|', debut);
        if (sep < 0) return false;
        champs[i] = ligne.substring(debut, sep);
        champs[i].trim();
        debut = sep + 1;
    }

    champs[5] = ligne.substring(debut);
    champs[5].trim();

    for (int i = 0; i < 6; i++) {
        if (champs[i].length() == 0) return false;
    }

    char reponse = rq_normaliserReponse(champs[5].charAt(0));

    q.enonce = champs[0];
    q.choix[0] = champs[1];
    q.choix[1] = champs[2];
    q.choix[2] = champs[3];
    q.choix[3] = champs[4];
    q.bonneReponse = reponse;

    return true;
}

static String rq_serialiserQuestion(const RQ_Question& q) {
    String ligne;
    ligne.reserve(
        q.enonce.length() +
        q.choix[0].length() +
        q.choix[1].length() +
        q.choix[2].length() +
        q.choix[3].length() + 16
    );

    ligne += rq_nettoyerChampPourFichier(q.enonce);
    ligne += '|';
    ligne += rq_nettoyerChampPourFichier(q.choix[0]);
    ligne += '|';
    ligne += rq_nettoyerChampPourFichier(q.choix[1]);
    ligne += '|';
    ligne += rq_nettoyerChampPourFichier(q.choix[2]);
    ligne += '|';
    ligne += rq_nettoyerChampPourFichier(q.choix[3]);
    ligne += '|';
    ligne += rq_normaliserReponse(q.bonneReponse);

    return ligne;
}

static bool rq_ecrireQuestionsDansLittleFS(const RQ_Question banque[], int count) {
    if (!rq_monterLittleFS()) return false;

    File file = LittleFS.open(RQ_FICHIER_QUESTIONS, "w");
    if (!file) {
        Serial.println("RQ: impossible d'ouvrir le fichier questions en ecriture");
        return false;
    }

    file.println("# Format : Enonce|Choix A|Choix B|Choix C|Choix D|Bonne reponse");
    file.println("# Bonne reponse : A, B, C ou D");

    for (int i = 0; i < count; i++) {
        file.println(rq_serialiserQuestion(banque[i]));
    }

    file.close();
    return true;
}

static bool rq_ecrireQuestionsDefautDansLittleFS() {
    RQ_Question banque[RQ_NB_DEFAUT];

    for (int i = 0; i < RQ_NB_DEFAUT; i++) {
        banque[i].enonce = RQ_DEFAUT[i][0];
        banque[i].choix[0] = RQ_DEFAUT[i][1];
        banque[i].choix[1] = RQ_DEFAUT[i][2];
        banque[i].choix[2] = RQ_DEFAUT[i][3];
        banque[i].choix[3] = RQ_DEFAUT[i][4];
        banque[i].bonneReponse = RQ_DEFAUT[i][5][0];
    }

    bool ok = rq_ecrireQuestionsDansLittleFS(banque, RQ_NB_DEFAUT);
    if (ok) {
        Serial.println("RQ: fichier questions par defaut cree dans LittleFS");
    }
    return ok;
}

static int rq_lireQuestionsDepuisNVS(RQ_Question banque[], int maxQuestions) {
    Preferences prefs;
    prefs.begin(RQ_NAMESPACE_NVS, true);

    int count = prefs.getInt("q_count", 0);
    if (count > maxQuestions) count = maxQuestions;

    for (int i = 0; i < count; i++) {
        banque[i].enonce   = prefs.getString(rq_cleQuestionNVS(i, "e").c_str(), "");
        banque[i].choix[0] = prefs.getString(rq_cleQuestionNVS(i, "a").c_str(), "");
        banque[i].choix[1] = prefs.getString(rq_cleQuestionNVS(i, "b").c_str(), "");
        banque[i].choix[2] = prefs.getString(rq_cleQuestionNVS(i, "c").c_str(), "");
        banque[i].choix[3] = prefs.getString(rq_cleQuestionNVS(i, "d").c_str(), "");
        String r = prefs.getString(rq_cleQuestionNVS(i, "r").c_str(), "A");
        banque[i].bonneReponse = rq_normaliserReponse(r.charAt(0));
    }

    prefs.end();
    return count;
}

static int rq_compterQuestionsLittleFS() {
    RQ_Question q;
    int count = 0;

    if (!rq_monterLittleFS()) return 0;

    File file = LittleFS.open(RQ_FICHIER_QUESTIONS, "r");
    if (!file) return 0;

    while (file.available()) {
        String ligne = file.readStringUntil('\n');
        if (rq_parserLigneQuestion(ligne, q)) count++;
    }

    file.close();
    return count;
}

static void rq_initialiserLeaderboardPourMode(int mode) {
    const char* ns = rq_namespacePourMode(mode);
    Preferences prefs;
    prefs.begin(ns, true);
    bool dejainit = prefs.getBool("scores_init", false);
    prefs.end();

    if (dejainit) return;

    rq_reinitialiserLeaderboard(mode);

    prefs.begin(ns, false);
    prefs.putBool("scores_init", true);
    prefs.end();
}

// ============================================================
// INITIALISATION
// ============================================================
void rq_initialiserFichiers() {
    if (!rq_monterLittleFS()) {
        Serial.println("RQ: LittleFS non disponible, questions non initialisees");
        for (int m = 0; m <= 2; m++) rq_initialiserLeaderboardPourMode(m);
        return;
    }

    int nbQuestions = rq_compterQuestionsLittleFS();

    if (nbQuestions > 0) {
        Serial.println("RQ: LittleFS OK, " + String(nbQuestions) + " questions dans " + RQ_FICHIER_QUESTIONS);
        for (int m = 0; m <= 2; m++) rq_initialiserLeaderboardPourMode(m);
        return;
    }

    // Si le fichier n'existe pas ou ne contient aucune question valide,
    // on tente d'abord de migrer les anciennes questions NVS.
    RQ_Question* anciennes = new RQ_Question[RQ_MAX_QUESTIONS];
    if (!anciennes) {
        Serial.println("RQ: ERREUR memoire migration questions");
        for (int m = 0; m <= 2; m++) rq_initialiserLeaderboardPourMode(m);
        return;
    }

    int nbAnciennes = rq_lireQuestionsDepuisNVS(anciennes, RQ_MAX_QUESTIONS);

    if (nbAnciennes > 0) {
        Serial.println("RQ: migration de " + String(nbAnciennes) + " questions NVS vers LittleFS");
        if (rq_ecrireQuestionsDansLittleFS(anciennes, nbAnciennes)) {
            Serial.println("RQ: migration terminee");
        } else {
            Serial.println("RQ: ERREUR migration NVS vers LittleFS");
        }
        delete[] anciennes;
        for (int m = 0; m <= 2; m++) rq_initialiserLeaderboardPourMode(m);
        return;
    }

    delete[] anciennes;

    // Dernier recours : creation du fichier avec les questions par defaut.
    Serial.println("RQ: aucune question trouvee, creation du fichier par defaut");
    rq_ecrireQuestionsDefautDansLittleFS();
    for (int m = 0; m <= 2; m++) rq_initialiserLeaderboardPourMode(m);
}

// ============================================================
// LEADERBOARD
// 3 namespaces NVS independants : rq_zen, rq_arc, rq_ms
// ============================================================
static const char* rq_namespacePourMode(int mode) {
    if (mode == 1) return "rq_arc";
    if (mode == 2) return "rq_ms";
    return "rq_zen";
}

void rq_chargerLeaderboard(RQ_Score table[], int mode) {
    Preferences prefs;
    prefs.begin(rq_namespacePourMode(mode), true);

    for (int i = 0; i < RQ_MAX_SCORES; i++) {
        String pseudo = prefs.getString(rq_cleScore(i, "p").c_str(), "---");
        rq_copierPseudo(table[i], pseudo.c_str());
        table[i].points = prefs.getInt(rq_cleScore(i, "v").c_str(), 0);
    }

    prefs.end();
}

void rq_sauvegarderLeaderboard(RQ_Score table[], int mode) {
    Preferences prefs;
    prefs.begin(rq_namespacePourMode(mode), false);

    for (int i = 0; i < RQ_MAX_SCORES; i++) {
        prefs.putString(rq_cleScore(i, "p").c_str(), table[i].pseudo);
        prefs.putInt(rq_cleScore(i, "v").c_str(), table[i].points);
    }

    prefs.putBool("scores_init", true);
    prefs.end();
}

void rq_reinitialiserLeaderboard(int mode) {
    RQ_Score vide[RQ_MAX_SCORES];

    for (int i = 0; i < RQ_MAX_SCORES; i++) {
        rq_copierPseudo(vide[i], "---");
        vide[i].points = 0;
    }

    rq_sauvegarderLeaderboard(vide, mode);
}

bool rq_insererScore(RQ_Score table[], const char* pseudo, int points, int mode) {
    if (points <= table[RQ_MAX_SCORES - 1].points &&
        table[RQ_MAX_SCORES - 1].points != 0) {
        return false;
    }

    rq_copierPseudo(table[RQ_MAX_SCORES - 1], pseudo);
    table[RQ_MAX_SCORES - 1].points = points;

    // Tri decroissant.
    for (int i = 0; i < RQ_MAX_SCORES - 1; i++) {
        for (int j = 0; j < RQ_MAX_SCORES - 1 - i; j++) {
            if (table[j].points < table[j + 1].points) {
                RQ_Score t = table[j];
                table[j] = table[j + 1];
                table[j + 1] = t;
            }
        }
    }

    rq_sauvegarderLeaderboard(table, mode);
    return true;
}

// ============================================================
// QUESTIONS
// Stockage LittleFS : /rq_questions.txt
// ============================================================
int rq_chargerQuestions(RQ_Question banque[]) {
    if (!rq_monterLittleFS()) return 0;

    File file = LittleFS.open(RQ_FICHIER_QUESTIONS, "r");
    if (!file) {
        Serial.println("RQ: fichier questions introuvable");
        return 0;
    }

    int count = 0;
    int ligneNumero = 0;

    while (file.available() && count < RQ_MAX_QUESTIONS) {
        ligneNumero++;
        String ligne = file.readStringUntil('\n');

        RQ_Question q;
        if (rq_parserLigneQuestion(ligne, q)) {
            banque[count++] = q;
        } else {
            ligne.trim();
            if (ligne.length() > 0 && !ligne.startsWith("#")) {
                Serial.println("RQ: ligne ignoree dans rq_questions.txt : " + String(ligneNumero));
            }
        }
    }

    file.close();

    Serial.println("RQ: " + String(count) + " questions chargees depuis LittleFS");
    return count;
}

bool rq_sauvegarderNouvelleQuestion(const RQ_Question& q) {
    RQ_Question* banque = new RQ_Question[RQ_MAX_QUESTIONS];
    if (!banque) {
        Serial.println("RQ: ERREUR memoire ajout question");
        return false;
    }

    int count = rq_chargerQuestions(banque);

    if (count >= RQ_MAX_QUESTIONS) {
        Serial.println("RQ: nombre maximum de questions atteint");
        delete[] banque;
        return false;
    }

    banque[count] = q;
    count++;

    bool ok = rq_ecrireQuestionsDansLittleFS(banque, count);
    delete[] banque;
    return ok;
}

bool rq_modifierQuestion(int index, const RQ_Question& q) {
    RQ_Question* banque = new RQ_Question[RQ_MAX_QUESTIONS];
    if (!banque) {
        Serial.println("RQ: ERREUR memoire modification question");
        return false;
    }

    int count = rq_chargerQuestions(banque);

    if (index < 0 || index >= count) {
        Serial.println("RQ: index de question invalide");
        delete[] banque;
        return false;
    }

    banque[index] = q;
    bool ok = rq_ecrireQuestionsDansLittleFS(banque, count);
    delete[] banque;
    return ok;
}

// ============================================================
// TIRAGE ALEATOIRE SANS REPETITION
// ============================================================
void rq_tirerQuestions(int totalBanque, int indices[], int nbATirer) {
    if (nbATirer > totalBanque) nbATirer = totalBanque;

    for (int i = 0; i < nbATirer; i++) {
        indices[i] = -1;
    }

    if (totalBanque <= 0) return;

    // Tableau temporaire de tous les index disponibles.
    // Allocation dynamique pour ne pas charger la pile.
    int* pool = new int[totalBanque];
    if (!pool) {
        Serial.println("RQ: ERREUR memoire tirage aleatoire");
        return;
    }

    for (int i = 0; i < totalBanque; i++) {
        pool[i] = i;
    }

    // Melange partiel Fisher-Yates.
    // esp_random() fournit une meilleure entropie sur ESP32 que analogRead().
    for (int i = 0; i < nbATirer; i++) {
        int j = i + (esp_random() % (totalBanque - i));

        int tmp = pool[i];
        pool[i] = pool[j];
        pool[j] = tmp;

        indices[i] = pool[i];
    }

    delete[] pool;

    Serial.print("RQ: tirage aleatoire = ");
    for (int i = 0; i < nbATirer; i++) {
        Serial.print(indices[i]);
        if (i < nbATirer - 1) Serial.print(",");
    }
    Serial.println();
}
