#ifndef DONNEES_RQ_H
#define DONNEES_RQ_H

// ============================================================
//  DONNEES_RQ.H
//  Stockage :
//  - Questions : LittleFS, fichier /rq_questions.txt
//  - Scores    : Preferences/NVS
// ============================================================

#include <Arduino.h>
#include <Preferences.h>

const int RQ_MAX_SCORES          = 10;
const int RQ_MAX_QUESTIONS       = 100;
const int RQ_NB_QUESTIONS_PARTIE = 10;
const int RQ_TEMPS_LIMITE_MS     = 10000;
const int RQ_MAX_PSEUDO          = 8;
const char* const RQ_MDP_ADMIN   = "1234";

struct RQ_Score {
    char pseudo[RQ_MAX_PSEUDO + 1];
    int  points;
};

struct RQ_Question {
    String enonce;
    String choix[4];
    char   bonneReponse;
};

void rq_initialiserFichiers();

void rq_chargerLeaderboard(RQ_Score table[]);
void rq_sauvegarderLeaderboard(RQ_Score table[]);
void rq_reinitialiserLeaderboard();
bool rq_insererScore(RQ_Score table[], const char* pseudo, int points);

int  rq_chargerQuestions(RQ_Question banque[]);
bool rq_sauvegarderNouvelleQuestion(const RQ_Question& q);
bool rq_modifierQuestion(int index, const RQ_Question& q);

void rq_tirerQuestions(int totalBanque, int indices[]);

#endif
