/*
CREATE TABLE Pontos (
  Id    INTEGER  PRIMARY KEY AUTOINCREMENT,
  x     REAL,
  y     REAL,
  tipo  CHAR (1) NOT NULL CHECK (tipo = '1' OR tipo = '3'),
  kva   REAL DEFAULT (0.0) NOT NULL
);

CREATE TABLE Sec (
  Id     INTEGER PRIMARY KEY NOT NULL,
  p1     INTEGER (2),
  p2     INTEGER (2),
  metros FLOAT (4),
  Cabo   INTEGER (2) NOT NULL
    REFERENCES Cabos (Id) ON DELETE RESTRICT
    ON UPDATE RESTRICT DEFERRABLE INITIALLY DEFERRED
)
WITHOUT ROWID;

CREATE VIRTUAL TABLE TEMP.rtree_index1 USING rtree_i32 (
  id,             -- Integer primary key
  minX, maxX,     -- Minimum and maximum X coordinate
  minY, maxY      -- Minimum and maximum Y coordinate
);

select * from rtree_index1 where minX > 300254 and maxX < 300256 and minY > 500058 and maxY < 500060;
*/

// Qt
#include <math.h>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QDebug>
#include <QApplication>
#include <QStringList>
#include <QSqlError>
#include <QElapsedTimer>
// C++
#include <iostream>
#include <fstream>
#include <stack>
#include "exception"
// Projeto
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "/home/john/Atalhos/Qt/QuedaTensao/CStack.h"
#include "/home/john/Atalhos/LocarPostes3/Geometria.h"
//--------------------------------------------------------------------------------------------------

#define LIMITE_ND22 350  // Item 6.1.4.1 ND 22 Elektro
// Copiado de CTabPostes. Ponto é um flying-tap.
#define atbFLYING_TAP 0x01
// Copiado de CTabPostes. Ponto é um poste de topo.
#define atbPOSTE_TOPO 0x04
// Ponto é um poste que integra um flying-tap
#define atbPOSTE_FT 0x02
#define DIST_MAX 277

typedef std::multimap<int, Arco>::iterator arcosIterator;

short in;
char breakpoint;
short pai;
void *gbPtr1, *gbPtr2;
//--------------------------------------------------------------------------------------------------

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
  ui->setupUi(this);
//  QPoint p = Segmentos::pontoIntersec(312648,7454724,312866,7454877,312632,7454876,312988,7454787);

  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");

  db.setDatabaseName("/home/john/Downloads/SQLiteStudio/teste.db");
  if (!db.open()) {
    QMessageBox::critical(nullptr, QObject::tr("Cannot open database"),
      QObject::tr("Unable to establish a database connection.\n"
      "This example needs SQLite support. Please read "
      "the Qt SQL driver documentation for information how "
      "to build it.\n\n"
      "Click Cancel to exit."), QMessageBox::Cancel);
    return;
  }

//  debug_var = 11;
  pontos = NULL;
  soma = 0;
//  kva_ramo1 = kva_ramo2 = 0;

  setlocale(LC_NUMERIC, "C");

  QSqlQuery query;
  if(query.exec(("CREATE VIRTUAL TABLE TEMP.rtree_index1 USING rtree_i32 ( "
    "id, "             // Integer primary key "
    "minX, maxX, "     // Minimum and maximum X coordinate "
    "minY, maxY);"))   // Minimum and maximum Y coordinate
     == false)
    qDebug() << "Falhou!";

  ui->btFuncao->setToolTip("K-Means modificado");
}
//--------------------------------------------------------------------------------------------------

MainWindow::~MainWindow()
{
  if(db.isOpen())
    db.close();
  delete ui;

  if(pontos != NULL)
    delete [] pontos;
}
//--------------------------------------------------------------------------------------------------

std::vector<Arco*> MainWindow::findArcos(int pontoId)
{
  int idx;
  std::vector<Arco*> lista;
/*
idx    noh      id
50	25	32
51	25	33
52	25	35
53	25	37
*/

  if((idx = arcosIndex.find_noh(pontoId)) >= 0) { // retorna 50
    int id = arcosIndex.get_id(idx); // retorna 32, 33, 35, 37
    while(id >= 0) {
      Arco *arco = arcos.search2(id);
      lista.push_back(arco);
      id = arcosIndex.get_id(); // retorna 32, 33, 35, 37
    }
  }

  return lista;
}
//--------------------------------------------------------------------------------------------------

/**
 * @brief Encontrar o ponto cujo id é passado.
 * @param id Id do ponto buscado.
 * @return Ponteiro para o objeto Ponto ou NULL.
 */
Ponto* MainWindow::findPonto(int id)
{
/*
  std::vector<int>::iterator it = std::find(index_pontos.begin(), index_pontos.end(), id);
  if(it != index_pontos.end()) {
    Ponto *ptr = &pontos[std::distance(index_pontos.begin(), it)];
    return ptr;
  }
*/
  if(id >= 0 && id < nPontos)
    return &pontos[id];
  return NULL;
}
//--------------------------------------------------------------------------------------------------

void MainWindow::on_pbLocaTrafos1_clicked()
{
qDebug() << "\r\nlocarTrafo";
  in = -1;
  CStack<int> vectra;
  gbPtr1 = &vectra;

  kva_total = pontos[36].kva_proprio;

  try {
    locarTrafo(36);
  }
  catch (EmptyStackException ex) {
    std::cerr << ex.what() << endl;
  }

//  while(!vectra.isempty()) {
  for(int k = 0; k < vectra.size(); k++)
    qDebug().noquote().nospace() << vectra.get(k);

  vectra.clear();
  gbPtr1 = NULL;
  close();
}
//--------------------------------------------------------------------------------------------------

/**
 * Linear Ordering Problem
 * https://addi.ehu.es/bitstream/handle/10810/11178/tr14-1b.pdf?sequence=4&isAllowed=y
 */
void MainWindow::lop()
{
  auto m = 5;
  Byte sigma[][5] = {
    1,2,3,4,5,
    2,3,1,4,5,
    5,3,4,2,1
  };
  Byte matriz[][5] = {
    0,16,11,15,7,
    21,0,14,15,9,
    26,23,0,26,12,
    22,22,11,0,13,
    30,28,25,24,0
  };

  char linha = 0;

//  for(auto i = 0; i < 3; i++) {
//    QDebug deb = qDebug().noquote().nospace();
//    for(auto j = 0; j < m; j++)
//      deb << " " << sigma[i][j];
//  }

  do {
    int soma = 0;
    for(auto row = 0; row < m - 1; row++) {
      for(auto col = row + 1; col < m; col++) {
        soma += matriz[sigma[linha][row]-1][sigma[linha][col]-1];
//        qDebug().noquote().nospace() <<
//          sigma[linha][row]-1 << ";" <<
//          sigma[linha][col]-1 << ";" << soma;
      }
    }

    qDebug().noquote().nospace() << "soma: " << soma;
  } while(++linha < 3);
}
//--------------------------------------------------------------------------------------------------

void MainWindow::locarTrafo(int id)
{
  pai = -1;

  kva_ramo1 = kva_ramo2 = 0;
  for(int i = 0; i < nPontos; i++) {
    pontos[i].visitado = false;
    pontos[i].kva_acumulado = 0;
  }

  locarTrafo3(-1, id, 0);

  qDebug()<< "";
//std::cout << std::endl << "==" << std::endl;

//  for(uint j = 0; j < nPontos; j++)
//    qDebug().noquote().nospace() << pontos[j].id << "; " << array_nos[pontos[j].id];
}
//------------------------------------- ------------------------------------------------------------

/**
 * MainWindow::locarTrafo3
 * @param target Próximo nó a visitar
 * @param dist
 */
void MainWindow::locarTrafo3(int pai, int current, float kva, float dist /*0*/)
{
  QString spc = "", str;
  in++;

  for(auto k = 0; k < in; k++)
    spc += "    ";

  CStack<int> *vectra = (CStack<int>*)gbPtr1;

  Ponto &refP1 = pontos[current];
//  qDebug().noquote().nospace() << str.sprintf("     c: %2d", refP1.id);
qDebug().noquote().nospace() << spc << str.sprintf("c: %2d", refP1.id);
  vectra->push(refP1.id);
qDebug().noquote().nospace() << str.sprintf("k: +%d", refP1.id);

//     qDebug() << "";
    for(int k = 0; k < vectra->size(); k++)
      qDebug().noquote().nospace() << "s: " << vectra->get(k);

  if(refP1.visitado == false) {
    int p1, p2;
    refP1.visitado = true;
    std::vector<Arco*> lista = findArcos(current);


//------------------------------------ LOOP -------------------------------------------- [
    for(uint j = 0; j < lista.size(); j++) {
      Arco *arco = lista[j];
      if(arco->p1 == current) { p1 = arco->p1; p2 = arco->p2; }
      else { p1 = arco->p2; p2 = arco->p1; } // inverter

      if(p2 == pai) {
//qDebug().noquote().nospace() << str.sprintf("     =: %2d", p2);
qDebug().noquote().nospace() << spc << str.sprintf("=: %2d", p2);
        continue;
      }
//qDebug().noquote().nospace() << str.sprintf("     p: %2d", p2);
qDebug().noquote().nospace() << spc << str.sprintf("p: %2d", p2); // Fim de linha
//qDebug().noquote().nospace() << spc << str.sprintf("r: %2d", lista.size() - j - 1); // Linhas restantes

      Ponto &refP2 = pontos[p2];
      if(refP2.visitado == true) {
        if(pontos[p1].kva_acumulado == 123)
          continue;
      }


      dist += arco->metros;
//      kva_total += refP2.kva_proprio;
      kva += refP2.kva_proprio;

//qDebug().noquote().nospace() << str.sprintf("%2d; %2d; %2d", pai, p1, p2);

      locarTrafo3(current, p2, kva, dist);
      dist -= arco->metros;
qDebug().noquote().nospace() << spc << str.sprintf("u: %2d", current);
qDebug().noquote().nospace() << spc << str.sprintf("j: %2d", j); // Linhas processadas
    } // for(int j = 0; j < lista.size(); j++)
//------------------------------------ FIM LOOP ---------------------------------------- ]

//*************************
  int popado = vectra->pop();
qDebug().noquote().nospace() << spc << str.sprintf("k: -%d", popado);

//    qDebug() << "";
    for(int k = 0; k < vectra->size(); k++)
      qDebug().noquote().nospace() << "s: " << vectra->get(k);
//*/

    if(lista.size() > 2)
      breakpoint = 1;
    else if(lista.size() == 1)
qDebug().noquote().nospace() << spc << str.sprintf("F: %2d", current); // Fim de linha

//    qDebug().noquote().nospace() << str.sprintf("     x: %2d", refP1.id); // eXit loop
//qDebug().noquote().nospace() << spc << str.sprintf("x: %2d", refP1.id); // Fim de linha
  } // if(refP1.visitado == false)

  else { // refP1.visitado == true
    breakpoint = 1;
    pontos[current].kva_acumulado = 123;
//??????????????????
  int popado = vectra->pop();
qDebug().noquote().nospace() << spc << str.sprintf("k: -%d", popado);

    for(int k = 0; k < vectra->size(); k++)
      qDebug().noquote().nospace() << "s: " << vectra->get(k);
//??????????????????
//------------------------------------ ANEL --------------------------------------------
/*
    if(pontos[current].tipo == '3') // É FT
// Indicar anel no pai, o nó antes do FT
      qDebug().noquote().nospace() << str.sprintf("   AnelA: %2d", pai);
    else // Não é FT
*/
      qDebug().noquote().nospace() << spc << str.sprintf("   Anel: %2d", current);
      int a = vectra->size();
      for(a = 0; vectra->get(a) != current && a < vectra->size(); a++);
      for(; a < vectra->size(); a++)
        qDebug().noquote().nospace() << "A: " << vectra->get(a);
      qDebug().noquote().nospace() << "A: " << current;
  }

  --in;
} // locarTrafo3(int, int, float, float)
//--------------------------------------------------------------------------------------------------

/**
 * Carregar os postes no arquivo Postes.csv para um vetor <b>std::vector<SPoste></b>.
 * @param vec_postes Referência a objeto <b>std::vector<SPoste></b> para armazenar os
 * postes carregados.
 * @param val Valor para inicialização do atributo <b>dist2Posto</b> dos postes.
 * Esse valor é diferente dependendo da função chamadora.
 */
void MainWindow::carregarPostes(std::vector<SPoste> &vec_postes, float val /*0*/)
{
  QString str;
  char buffer[128];
  std::ifstream is;

/* Postes.csv
 * Não pode haver linha com títulos de colunas, a não ser como comentário.
 * USAR PONTO DECIMAL '.'
 *
 * Arquivo deve estar ordenado por pontoId.
 * Arauivo chama Postes.csv, mas tam flying-taps também.
 *
 *   1;314136.7330220111;7454399.523036179;173;12;3.58185097240589
 *   2;314156.209140579;7454457.567675321;148;12;0.237909525964266
 *   3;314153.92889885226;7454321.917092844;75;22;1.70673326373797
 *   | |                  |                 |  |  |
 * 5 | |                  |                 |  |  kva
 * 4 | |                  |                 |  atribs
 * 3 | |                  |                 angulo
 * 2 | |                  y
 * 1 | x
 * 0 Id
 * Metragem
 * Somatória (distância de cada poste ao trafo * carga de cada poste)
 */
  is.open("/home/john/Atalhos/Qt/QuedaTensao/Dados/Postes.csv");
  if(!is.is_open()) {
    std::cout << "File /home/john/Atalhos/Qt/QuedaTensao/Dados/Postes.csv not found." << endl;
    return;
  }

//  nPontos = 0;
  SPoste poste;

  while(!is.eof()) {
    is.getline(buffer, 128);
    if(buffer[0] == '/' && buffer[1] == '/') // comentário
      continue;
    str = buffer;
    if(str.isEmpty())
      continue;

    QStringList stringList = str.split(";");
    poste.id = stringList.at(0).toInt();
if(poste.id == 198)
  breakpoint = 1;
    poste.x = stringList.at(1).toDouble();
    poste.y = stringList.at(2).toDouble();
//      poste.ang = stringList.at(3).toInt();
    poste.atribs = stringList.at(4).toInt();
    poste.kva = stringList.at(5).toFloat();
//    poste.kva = 1.0;
    poste.posto = -2;
    poste.dist2Posto = val;
//    poste.prodDistKva = val;
    vec_postes.push_back(poste);
//qDebug().noquote().nospace() << id << ", " << x << ", "  << y;
  } // while(!is.eof())

  is.close();
} // carregarPostes(std::vector<SPoste>&, float)
//--------------------------------------------------------------------------------------------------

void MainWindow::carregarArcos()
{
  QString str;
  char buffer[128];
  std::ifstream is;

/* arcos.csv
 * Não pode haver linha com títulos de colunas, a não ser como comentário.
 * USAR PONTO DECIMAL '.'
 *
 * A ordem de p1 e p2 em cada linha não importa. Pode ser p1,p2 ou p2,p1. Por exemplo, a
 * primeira linha a seguir pode ser 01;<b>13;04</b>;21,42;04;01 ou 01;<b>04;13</b>;21,42;04;01
 *
 * 182;1;26;23.2662199777218
 * 155;1;59;29.9431457536053
 * 183;2;32;26.648920727179
 * 153;2;58;27.6874594287174
 * |   | |  |
 * |   | |  metros
 * |   | p2
 * |  p1
 * rowid
 */

  is.open("/home/john/Atalhos/Qt/QuedaTensao/Dados/arcos.csv");
  if(!is.is_open()) {
    std::cout << "File /home/john/Atalhos/Qt/QuedaTensao/Dados/Dados/arcos.csv not found." << endl;
    return;
  }

  Arco arco;
  int n1, n2, rowid;
  arcos.clear();
  arcosIndex.clear();

  while(!is.eof()) {
    is.getline(buffer, 128);

    if(buffer[0] == '/' && buffer[1] == '/') // comentário
      continue;

    str = buffer;
    if(str.isEmpty())
      continue;

    QStringList stringList = str.split(";");
    rowid = stringList.at(0).toInt();
    n1 = stringList.at(1).toInt();
    n2 = stringList.at(2).toInt();

    arco.id = rowid;
    arco.p1 = n1;
    arco.p2 = n2;
    arco.metros = stringList.at(3).toFloat();
    arco.qtKva100m = 0;
    arco.visitado = false;
    arcos.push(rowid, arco);
    arcosIndex.push_back(n1, rowid);
    arcosIndex.push_back(n2, rowid);
//qDebug().noquote() << str.sprintf("%2d %2d", p1, p2);
  }

  is.close();

  arcosIndex.sort_pares();
} // carregarArcos()
//--------------------------------------------------------------------------------------------------

/**
 * Determinar o centro de carga aproximado de um circuito secundário.
 * Ver <b>Centro de carga</b> em <b>Locação de postes.odt</b>.
 *
 * @param nohs Relação dos identificadores dos postes atribuidos a um posto,
 * atribuição feita em <b>modifiedKmeans()</b>.
 * @return Identificador do poste escolhido como centro de carga.
 */
int MainWindow::centroDeCarga(std::vector<int> nohs)
{
  Arco *pArco;
  int szNohs = nohs.size();
  std::vector<SPoste> *vec_postes = (std::vector<SPoste>*)gbPtr1;

  QString qString;
  float menorDif = 99999;
  int idPostoMenorDif = -1;
  int breakpoint2;
  float kg, kvaGlobal;

  for(int s = 0; s < szNohs; s++) {
    int id_posto = nohs[s];
if(id_posto == 28)
  breakpoint2 = 0;

    std::vector<Arco*> lista = findArcos(id_posto);

    if(lista.size() == 2) {
      for(int x = 0; x < szNohs; x++)
        vec_postes->at(nohs[x] - 1).posto = id_posto;

      kg = 0;
      float kvaTotal;
      double produto[2] = { 0, 0 };

      for(char k = 0; k < 2; k++) {
        pArco = lista[k];
        int p2 = pArco->p1 == id_posto?pArco->p2:pArco->p1;
        kvaTotal = 0;
        centroDeCarga(id_posto, p2, id_posto, kvaTotal, pArco->metros);
        kg += kvaTotal;

        produto[k] = 0;
if(id_posto == breakpoint2)
        qDebug().noquote() << "posto: " << id_posto;

        for(int l = 0; l < szNohs; l++) {
          SPoste &p = vec_postes->at(nohs[l] - 1);
//          produto[k] += p.dist2Posto * p.kva;
          produto[k] += p.kva;
          p.dist2Posto = 0;
        } // for(int l = 0; l < szNohs; l++)

if(id_posto == breakpoint2)
        qDebug().noquote() << qString.sprintf("kvaTotal: %.2f", kvaTotal);

        if(kvaTotal > 0)
          produto[k] /= kvaTotal;
      } // (int k = 0; k < 2; k++)

      float dif = fabs(produto[0] - produto[1]);
      if(dif < menorDif) {
//        if(dif == 0) return id_posto;
//        if(dif < 10) break;
        menorDif = dif;
        idPostoMenorDif = id_posto;
        kvaGlobal = kg;
        if(dif == 0) break;
      } // if(dif < menorDif)
    } // if(lista.size() == 2)
  } // for(int s = 0; s < szNohs; s++)

  qString.sprintf("posto: %3d;%.2f kVA", idPostoMenorDif, kvaGlobal);
  qDebug().noquote() << qString;

  for(int s = 0; s < szNohs; s++)
    vec_postes->at(nohs[s] - 1).posto = idPostoMenorDif;

  return idPostoMenorDif;
} // void MainWindow::centroDeCarga(int[])
//------------------------------------- ------------------------------------------------------------

float MainWindow::centroDeCarga(int anterior, int current,
  int id_posto, float &kvaTotal, float distancia /*0*/)
{
  std::vector<SPoste> *vec_postes = (std::vector<SPoste>*)gbPtr1;
  SPoste &refP1 = vec_postes->at(current - 1);

  if(refP1.posto == id_posto) {
    int p1, p2;
    Arco *arco;
//    QString qString;

    std::vector<int> *vec_sec = (std::vector<int>*)gbPtr2;

    refP1.posto = id_posto;
    refP1.dist2Posto = distancia;
//    refP1.prodDistKva = distancia * refP1.kva;
    kvaTotal += refP1.kva;

    std::vector<Arco*> lista = findArcos(current);
//qDebug().noquote().nospace() << qString.sprintf("%3d;%6.2f;%6.2f", current, distancia, kvaTotal);

    if(lista.size() == 1) {
// Fim da linha
      arco = lista[0];
      p2 = arco->p1 == current?arco->p2:arco->p1;
//qDebug().noquote().nospace() << current << ", " <<  p2;

      if(p2 == anterior)
breakpoint = 1;
      else {
        SPoste &refP2 = vec_postes->at(p2 - 1);
        kvaTotal += refP2.kva;
        float d = distancia + arco->metros;

        if(d <= LIMITE_ND22) { // Item 6.1.4.1 ND 22 Elektro
  //        refP1.posto = id_posto;
//qDebug().noquote().nospace() << qString.sprintf("A;%3d;%3d", current, p2);
          vec_sec->push_back(current);
          vec_sec->push_back(p2);
//          kvaTotal += refP2.kva;
  //        distancia = d;
  //qDebug().noquote().nospace() << qString.sprintf("%3d;%6.2f;%6.2f", p2, distancia, kvaTotal);
        }
      } // if(p2 != anterior)
    } // if(lista.size() == 1)


    else { // lista.size() > 1
      for(int j = 0; j < lista.size(); j++) {
        arco = lista[j];
        if(arco->p1 == current) { p1 = arco->p1; p2 = arco->p2; }
        else { p1 = arco->p2; p2 = arco->p1; } // inverter

        if(p2 == anterior) // está querendo voltar, não pode
          continue;

        SPoste &refP2 = vec_postes->at(p2 - 1);
        float d = distancia + arco->metros;

        if(d <= LIMITE_ND22 // Item 6.1.4.1 ND 22 Elektro
          || (refP1.atribs & (atbFLYING_TAP | atbPOSTE_TOPO)) != 0) { // É flying-tap ou poste de topo
// Se o ponto é flying-tap, LIMITE_ND22 pode ser ultrapassado para que não fique incompleto.
// Se o ponto é poste de topo, contrói pelo menos um vão de cada lado, isto é, se veio da rua
// que termina na do poste de topo, contrói pelo menos um vão de cada lado nessa rua, mesmo que
// LIMITE_ND22 seja ultrapassado.
          vec_sec->push_back(current);
          vec_sec->push_back(p2);
//          kvaTotal += refP2.kva;
//          distancia = d;

          float deflex = -99;
          if(refP1.atribs & atbFLYING_TAP) {// || refP1.atribs & atbPOSTE_TOPO) {
            SPoste p0 = vec_postes->at(anterior - 1);
            deflex = Geometria::deflexao(p0.x, p0.y, refP1.x, refP1.y, refP2.x, refP2.y);
          }

//          if(deflex < -45 || deflex > 45) {
            centroDeCarga(current, p2, id_posto, kvaTotal, d);
breakpoint = 1;
//  if(kvaTotal > 22.5)
//    break;
//          }
        }
      } // for(int j = 0; j < lista.size(); j++)
    } // else (lista.size() > 1)

    return distancia;
  } // if(refP1.posto == 0)
} // centroDeCarga(int, int, int, float&, float)
//--------------------------------------------------------------------------------------------------

#ifdef KMEANS
void MainWindow::on_btKMeans_clicked()
{
  QElapsedTimer timer;
  timer.start();
//  for(int i = 0; i < 1000; i++)
    kmeans1();
  qDebug() << "The kmeans process took" << timer.elapsed()/1000. << "seconds";
}
//--------------------------------------------------------------------------------------------------

void MainWindow::kmeans1()
{
  QString str;
  char buffer[64];
  std::ifstream is;

/* cargas.csv
 * Não pode haver linha com títulos de colunas, a não ser como comentário.
 * USAR PONTO DECIMAL '.'
 *
 * 1;0;240
 * 2;20;240
 * 3;40;240
 * 4;60;240
 * | |  |
 * | |  y
 * | x
 * id
 */

  int i = 0;

//  is.open("/home/john/Atalhos/Qt/QuedaTensao/Dados/cargas.csv");
//  is.open("/home/john/Atalhos/Qt/QuedaTensao/Dados/Postes Itatiba.csv");

// Carregar cargas do arquivo PostesIta.csv [
  is.open("/home/john/Atalhos/Qt/QuedaTensao/Dados/PostesIta.csv");

  if(is.is_open()) {
    int id;
    float x, y;

    ncargas = 0;

    while(!is.eof()) {
      is.getline(buffer, 128);
      QString str = buffer;
      if(str.isEmpty())
        break;

      QStringList stringList = str.split(";");
      id = stringList.at(0).toInt();
      x = stringList.at(1).toFloat();
      y = stringList.at(2).toFloat();
      cargas[i].cluster = 0;
      cargas[i].x = x;
      cargas[i++].y = y;
    }

    ncargas = i;
    is.close();
  } // if(is.is_open())

  else {
    qDebug().noquote() << "Arquivo não encontrado";
    return;
  }
// Carregar cargas do arquivo PostesIta.csv ]

// Carregar clusters do arquivo "Clusters Itatiba.csv" [
  is.open("/home/john/Atalhos/Qt/QuedaTensao/Dados/Clusters Itatiba.csv");

  if(is.is_open()) {
    float x, y;

    for(i = 0; i < NUM_CLUSTERS && !is.eof(); i++) {
      is.getline(buffer, 64);
      QString str = buffer;
      if(str.isEmpty())
        break;

      QStringList stringList = str.split(";");
      x = stringList.at(0).toFloat();
      y = stringList.at(1).toFloat();
      clusters[i].x = x;
      clusters[i].y = y;
    }

    is.close();
  }
// Carregar clusters do arquivo "Clusters Itatiba.csv" ]


//  short contagem[NUM_CLUSTERS]; // número de cargas nos clusters

// Atribuir as cargas aos clusters até não haver mais trocas
  kmeans2();

// Preparar arquivo "kmeans_csv2dxf.csv" pasra posterior conversão em dxf [
  FILE *fp = fopen("/home/john/Atalhos/Qt/csv2dxf/Debug/kmeans_csv2dxf.csv", "w");
  fprintf(fp, "layer;kmeans;yellow;0\r\n");

  for(int j = 0; j < NUM_CLUSTERS; j++) {
    short cor = j;
//    contagem[j] = 0;
    fprintf(fp, "circle;%.6f;%.6f;8;kmeans;%d\r\n", clusters[j].x, clusters[j].y, cor);

    for(int i = 0; i < ncargas; i++) {
      if(cargas[i].cluster == j) {
//        contagem[j]++; // número de cargas no cluster j
        fprintf(fp, "line;%.6f;%.6f;%.6f;%.6f;kmeans;%d\r\n",
          clusters[j].x, clusters[j].y, cargas[i].x, cargas[i].y, cor);
        fprintf(fp, "circle;%.6f;%.6f;2;kmeans;%d\r\n", cargas[i].x, cargas[i].y, cor);
      }
    } // for(int i = 0; i < ncargas; i++)
// Preparar arquivo "kmeans_csv2dxf.csv" pasra posterior conversão em dxf ]

/*
    qDebug().noquote() << " ";

    for(int j = 0; j < NUM_CLUSTERS; j++)
      qDebug().noquote().nospace() << str.sprintf("%d: %3d %9.2f %10.2f", j,
        contagem[j], clusters[j].x, clusters[j].y);
//*/
  } // for(int j = 0; j < NUM_CLUSTERS; j++)

  fclose(fp);
  close();
}
//--------------------------------------------------------------------------------------------------

int MainWindow::kmeans2()
{
  QString str;
  int ntrocas;
  int clusterEscolhido;
  bool houveTroca[NUM_CLUSTERS]{false};

  do {
    ntrocas = 0;
//    std::fill_n(houveTroca, NUM_CLUSTERS, false);

    for(int i = 0; i < ncargas; i++) {
      double menorDist = 999999999E99;

// Encontrar a menor distância da cargas[i] a um cluster e atribuir o cluster à carga [
      for(int j = 0; j < NUM_CLUSTERS; j++) {
        double dist = pow(clusters[j].x - cargas[i].x, 2) + pow(clusters[j].y - cargas[i].y, 2);
        if(dist < menorDist) {
          menorDist = dist;
          clusterEscolhido = j;
        }
      } // for(int j = 0; j < NUM_CLUSTERS; j++)
// Encontrar a menor distância da cargas[i] a um cluster e atribuir o cluster à carga ]

// Se cargas[i] mudou de cluster contabilizar uma troca [
      if(cargas[i].cluster != clusterEscolhido) {
        houveTroca[cargas[i].cluster] = true;
        houveTroca[clusterEscolhido] = true;
        cargas[i].cluster = clusterEscolhido;
        ntrocas++;

//      qDebug().noquote().nospace() << str.sprintf("c = %3d i = %3d %6.2f, %6.2f",
//        clusterEscolhido, i, cargas[i].x, cargas[i].y);
      } // if(cargas[i].cluster != clusterEscolhido)
// Se cargas[i] mudou de cluster contabilizar uma troca ]

    } // for(int i = 0; i < ncargas; i++)

// Recalcular o centroide dos clusters nos quais ocorreram trocas (perderam ou ganharam cargas) [
    for(int j = 0; j < NUM_CLUSTERS; j++) {
      if(houveTroca[j]) {
        int n = 0;
        double total_x = 0;
        double total_y = 0;

        houveTroca[j] = false;

        for(int i = 0; i < ncargas; i++) {
          if(cargas[i].cluster == j) {
            n++;
            total_x += cargas[i].x;
            total_y += cargas[i].y;
          }
        } // for(int i = 0; i < ncargas; i++)

        if(n > 0) {
// Nova posição do centroide
          clusters[j].x = total_x / n;
          clusters[j].y = total_y / n;
        }

//    qDebug().noquote().nospace() << str.sprintf("%.2f, %.2f", clusters[j].x, clusters[j].y);
      } // if(houveTroca[j])
    } // for(int j = 0; j < NUM_CLUSTERS; j++)
// Recalcular o centroide dos clusters nos quais ocorreram trocas (perderam ou ganharam cargas) ]

  } while(ntrocas > 0);

//  qDebug().noquote() << "";
  return ntrocas;
}
//--------------------------------------------------------------------------------------------------

/*a*
 * MainWindow::on_pbLocaTrafos_clicked()
 *
 * Usa e cargas.csv e arcos2.csv
 *a/
void MainWindow::on_pbLocaTrafos_clicked()
{
  QString str;
  char buffer[64];
  std::ifstream is;

/a* cargas.csv
 * Não pode haver linha com títulos de colunas, a não ser como comentário.
 * USAR PONTO DECIMAL '.'
 *
 * 1;0;240
 * 2;20;240
 * 3;40;240
 * 4;60;240
 * | |  |
 * | |  y
 * | x
 * id
 *a/

  int i = 0;

  is.open("/home/john/Atalhos/Qt/QuedaTensao/Dados/PostesIta.csv");

  if(is.is_open()) {
    int id;
    double x, y;

    ncargas = 0;

    while(!is.eof()) {
      is.getline(buffer, 128);
      QString str = buffer;
      if(str.isEmpty())
        break;

      QStringList stringList = str.split(";");
      id = stringList.at(0).toInt();
      x = stringList.at(1).toDouble();
      y = stringList.at(2).toDouble();
      cargas[i].id = id;
      cargas[i].cluster = 0;
      cargas[i].x = x;
      cargas[i].y = y;
      cargas[i].visitado = 0;
      cargas[i++].squaredDistAcum = 999999999E99;

      ncargas++;
    }

    is.close();
  }

/a*
 * 1;2
 * 1;22
 * 2;1
 * | |
 * | p2
 * p1
 *a/

  is.open("/home/john/Atalhos/Qt/QuedaTensao/Dados/ArcosIta.csv");

  if(is.is_open()) {
    int p1, p2;

    while(!is.eof()) {
      is.getline(buffer, 128);
      QString str = buffer;
      if(str.isEmpty())
        break;

      QStringList stringList = str.split(";");
      p1 = stringList.at(0).toInt();
      p2 = stringList.at(1).toInt();
      ArcoKMeans arco(p1, p2);
      arcos2.push(p1, arco);
    }

    is.close();
  }

  clusters[0].cargaId = 221;
  clusters[1].cargaId = 168;
  clusters[2].cargaId = 73;
  clusters[3].cargaId = 132;
  clusters[4].cargaId = 182;
  clusters[5].cargaId = 134;
  clusters[6].cargaId = 117;
  clusters[7].cargaId = 4;

//~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~


//  for(char i = 0; i < NUM_CLUSTERS; i++) {
//    clusters[i].cargaId = i << 1; // Posição inicial
//    clusters[i].ncargas = 0;
//  }



  for(int k = 0; k < ncargas; k++)
    cargas[k].cluster = -1;

//  cargas[0].cluster = 0;
//  cargas[8].cluster = 1;

  do {
    houveTroca = false;

    for(short clusterId = 0; clusterId < NUM_CLUSTERS; clusterId++) {
      SPontoCarga &c = cargas[clusters[clusterId].cargaId - 1];
      c.squaredDistAcum = 0;

      for(int k = 0; k < ncargas; k++) {
        cargas[k].visitado = 0;
//        cargas[k].cluster = -1;
      }
qDebug().noquote() << "\r\npercorrer " << clusterId;
      arcosVisitados.clear();
      cargas[c.id].cluster = clusterId;
      percorrer(c, clusterId);
    } // for(short clusterId = 0; clusterId < NUM_CLUSTERS; clusterId++)

    for(short clusterId = 0; clusterId < NUM_CLUSTERS; clusterId++) {
qDebug().noquote() << "\r\nclusterId: " << clusterId;
      int n = 0;
      double centroid_x = 0, centroid_y = 0;

      for(int k = 0; k < ncargas; k++) {
        SPontoCarga &c = cargas[k];
        if(c.cluster == clusterId) {
qDebug().noquote() << c.id;
          n++;
          centroid_x += c.x;
          centroid_y += c.y;
        }
      }

      if(n > 0) {
//        clusters[clusterId].x = centroid_x / n;
//        clusters[clusterId].y = centroid_y / n;
        centroid_x /= n;
        centroid_y /= n;

        int cargaId = clusters[clusterId].cargaId;
//        double maiorDist = 999999999E99;
        double menorDist = pow(centroid_x - cargas[cargaId].x, 2) + pow(centroid_y - cargas[cargaId].y, 2);

        for(int m = 0; m < ncargas; m++) {
          if(cargas[m].cluster == clusterId) {
            double dist = pow(centroid_x - cargas[m].x, 2) + pow(centroid_y - cargas[m].y, 2);
            if(dist < menorDist) {
              menorDist = dist;
              cargaId = m;
            }
          }
        }

        cargas[cargaId].cluster = clusterId;
        clusters[clusterId].cargaId = cargaId;
        clusters[clusterId].x = cargas[cargaId].x;
        clusters[clusterId].y = cargas[cargaId].y;
qDebug().noquote() << str.sprintf("P%d < C%d", cargaId, clusterId);
      } // if(n > 0)
    } // for(char clusterId = 0; clusterId < NUM_CLUSTERS; clusterId++)

//houveTroca = 0;
  } while(houveTroca);

  FILE *fp = fopen("/home/john/Atalhos/Qt/csv2dxf/Debug/kmeans_csv2dxf.csv", "w");
  fprintf(fp, "layer;kmeans;yellow;0\r\n");

  ArcoKMeans *arcoArray = arcos2.getData();

  for(uint a = 0; a < arcos2.size(); a++) {
    ArcoKMeans arco = arcoArray[a];
    int p1 = arco->p1;
    int p2 = arco->p2;

    if(cargas[p1].cluster == cargas[p2].cluster) {
      short cor = cargas[p1].cluster; // cargas[p1].cluster (clusterId) usado como número da cor

qDebug().noquote().nospace() << str.sprintf("%3d %3d %6.2f, %6.2f %6.2f, %6.2f",
  p1, p2, cargas[p1].x, cargas[p1].y, cargas[p2].x, cargas[p2].y);

      if(cargas[p1].cluster >= 0)
        fprintf(fp, "line;%.6f;%.6f;%.6f;%.6f;kmeans;%d\r\n",
          cargas[p1].x, cargas[p1].y, cargas[p2].x, cargas[p2].y, cor);
    }
  }

  for(int clusterId = 0; clusterId < NUM_CLUSTERS; clusterId++) {
    short cor = clusterId;

    qDebug().noquote() << clusterId << " " << clusters[clusterId].cargaId;
// clusterId usado como número da cor
    fprintf(fp, "circle;%.6f;%.6f;8;kmeans;%d\r\n",
      clusters[clusterId].x, clusters[clusterId].y, cor);
  }

  fclose(fp);

  qDebug().noquote() << " ";
  for(int m = 0; m < ncargas; m++)
    qDebug().noquote().nospace() << str.sprintf("%3d %3d", m, cargas[m].cluster);

  close();
}*/
//--------------------------------------------------------------------------------------------------

/**
 * @brief MainWindow::percorrer
 * Antes da primeira chamada atribuir SPontoCarga[j].squaredDistAcum <- 999999999E99
 * Chamar uma vez para cada cluster.
 * Antes de cada chamada limpar cargas[i].visitado
 * Cada chamada atribuirá a SPontoCarga[j].dist2Cluster um novo valor se menor que o anterior
 * @param p1
 */
bool MainWindow::percorrer(SPontoCarga &c1, int clusterId, int anterior /* -1 */)
{
  QString str;
//  if(c1.visitado == false) {
//    c1.visitado = true;

  std::vector<ArcoKMeans> list = arcos2.search(c1.id);

  for(char k = 0; k < list.size(); k++) {
    ArcoKMeans arco = list[k];

    if(arco.p2 != anterior) {
    SPontoCarga &c2 = cargas[arco.p2 - 1];

//if(c2.id == 11)
//  qDebug().noquote() << " ";

    bool visitado = false;
    auto itr = arcosVisitados.equal_range(c1.id);
    for (auto it = itr.first; it != itr.second; it++) {
      if(it->second == c2.id) {
        visitado = true;
        break;
      }
    }

    if(visitado == true) continue;

    arcosVisitados.insert({c1.id, c2.id});
//      arcosVisitados.insert({c2.id, c1.id});
qDebug().noquote() << c1.id << " " << c2.id;

    double squaredDist = pow(c1.x - c2.x, 2) + pow(c1.y - c2.y, 2);
    double squaredDistAcum = c1.squaredDistAcum + squaredDist;
    if(squaredDistAcum < c2.squaredDistAcum) {
      c2.squaredDistAcum = squaredDistAcum;
      if(c2.cluster != clusterId) {
        c2.cluster = clusterId;
        houveTroca = true;
//qDebug().noquote() << c2.id << " <- " << clusterId;
qDebug().noquote() << str.sprintf("C%d < P%d", clusterId, c2.id);
      }
    }
//        else
//          return false;

    percorrer(c2, clusterId, c1.id);
    }
  } // for(char i = 0; i < list.size(); i++)

  return false;
//  } // if(c1.visitado == false)

  return true;
}
#endif
//--------------------------------------------------------------------------------------------------

void MainWindow::interpolacao1()
{
  QString str;
  char buffer[128];
  std::ifstream is;
  int x = 0, y = 0;

  struct SPoint {
    int x, y;
  };

  struct CCargas {
    int x, y;
    float kva;
  } cargas[400][400];

/* 13;30;390;4.2
 * 14;30;420;0.68
 * 15;30;450;0.61
 * |  |  |   |
 * |  |  |   kva
 * |  |  y
 * |  x
 * id
 */
  is.open("/home/john/Atalhos/Qt/QuedaTensao/Dados/bitmap.txt");
  if(is.is_open()) {
// 400 x 400 pixels  => 1600 linhas
    for(auto col = 0; col < 400; col++) {
      for(auto row = 0; row < 400; row++) {
        while(!is.eof()) {
          is.getline(buffer, 128);
          if(buffer[0] == '/' && buffer[1] == '/') // comentário
            continue;
          QString str = buffer;
          if(str.isEmpty())
            break;

//qDebug().noquote().nospace() << str;
//std::cout << col << ";" << row << std::endl;

          QStringList stringList = str.split(";");
          cargas[col][row].x = stringList.at(1).toInt();
          cargas[col][row].y = stringList.at(2).toInt();
          cargas[col][row].kva = stringList.at(3).toFloat();
//qDebug().noquote().nospace() << str.sprintf("%d;%d;%.2f",
//  cargas[col][row].x, cargas[col][row].y, cargas[col][row].kva);
          break;
        } // while(!is.eof())
      } // for(auto row = 0; row < 40; row++)
    } // for(auto col = 0; col < 40; col++)

    is.close();

    float val;
    SPoint posto, carga;
    float relevo[400][400];

    for(auto j = 0; j < 400; j++) {
      for(auto k = 0; k < 400; k++) {

        relevo[j][k] = 0;

// Coordenadas do eventual posto
        posto.x = cargas[j][k].x;
        posto.y = cargas[j][k].y;

        for(auto col = 0; col < 400; col++) {
          for(auto row = 0; row < 400; row++) {

// Coordenadas da carga
            carga.x = cargas[col][row].x;
            carga.y = cargas[col][row].y;

// Quadrado da distância do posto à carga
            float dist2 = pow(posto.x - carga.x, 2) + pow(posto.y - carga.y, 2);

            if(dist2 != 0 && dist2 < 10000) {
// Só consideradas as cargas a menos de 100 m do posto
              val = cargas[col][row].kva / dist2;
              relevo[j][k] += val;
            }
          }
        }
      }
    }

    for(auto col = 0; col < 400; col++)
      for(auto row = 0; row < 400; row++) {
        if(relevo[col][row] > 60000)
          std::cout << col << ";" << row << ";" << relevo[col][row] << std::endl;
      }
  } // if(is.is_open())

  std::cout << std::endl << "FIM" << std::endl;
  close();
}
//--------------------------------------------------------------------------------------------------

/**
 *
 */
void MainWindow::interpolacao2()
{
  QString str;
  char buffer[128];
  std::ifstream is;

  struct SPoste {
    int id;
    double x, y;
    short ang;
    Byte atribs;
    float kva;
  };

  struct SPoint { double x, y; };

/* Postes.csv
 * Não pode haver linha com títulos de colunas, a não ser como comentário.
 * USAR PONTO DECIMAL '.'
 *
 * Arquivo deve estar ordenado por pontoId, isto é, a primeira linha deve corresponder ao ponto 0,
 * a segunda ao ponto 1 e assim por diante até n-1, onde n é o número de pontos.
 *
 * // rowid;X;Y;int(angulo + 0.5);atribs;carga
 *   1;314136.7330220111;7454399.523036179;173;12;3.58185097240589
 *   2;314156.209140579;7454457.567675321;148;12;0.237909525964266
 *   3;314153.92889885226;7454321.917092844;75;22;1.70673326373797
 *   | |                  |                 |  |  |
 * 5 | |                  |                 |  |  kva
 * 4 | |                  |                 |  atribs
 * 3 | |                  |                 angulo
 * 2 | |                  y
 * 1 | x
 * 0 Id
 */
  is.open("/home/john/Atalhos/Qt/QuedaTensao/Dados/Postes.csv");
  if(is.is_open()) {
    SPoste poste;
    std::vector<SPoste> vec_postes;

    while(!is.eof()) {
      is.getline(buffer, 128);
      if(buffer[0] == '/' && buffer[1] == '/') // comentário
        continue;
      str = buffer;
      if(str.isEmpty())
        continue;

      QStringList stringList = str.split(";");
      poste.id = stringList.at(0).toInt();
      poste.x = stringList.at(1).toDouble();
      poste.y = stringList.at(2).toDouble();
      poste.ang = stringList.at(3).toInt();
      poste.atribs = stringList.at(4).toInt();
      poste.kva = stringList.at(5).toFloat();
      vec_postes.push_back(poste);
//qDebug().noquote().nospace() << id << ", " << x << ", "  << y;
    } // while(!is.eof())

    is.close();
//---------------------------------------------

    SPoint posto;
    float squared_dist;
// Relevo 3D virtual com pontos mais altos onde há maior concentração de carga;
    float *relevo = new float[vec_postes.size()];

    for(auto j = 0; j < vec_postes.size(); j++) {
      relevo[j] = 0;
      posto.x = vec_postes[j].x;
      posto.y = vec_postes[j].y;

      for(auto k = 0; k < vec_postes.size(); k++) {
        if(k != j) {
// Distância euclidiana do posto candidato em vec_postes[j] até o poste em vec_postes[k]
//          squared_dist = sqrt(pow(posto.x - vec_postes[k].x, 2) + pow(posto.y - vec_postes[k].y, 2));
	  squared_dist = pow(posto.x - vec_postes[k].x, 2) + pow(posto.y - vec_postes[k].y, 2);

	  if(squared_dist < 22500) { // Só consideradas as cargas em postes a menos de 150 m do posto
	    double kva_div_m2 = vec_postes[k].kva / squared_dist;
	    relevo[j] += kva_div_m2;
	  } // if(dist < 22500)
	} // if(k != j)
      } // for(auto k = 0; k < vec_postes.size(); k++)
    } // for(auto j = 0; j < vec_postes.size(); j++)


/* Para cada ponto candidato a posto transformador, buscar outro dentro de um círculo
 * de determinado raio e comparar os kVA's de ambos. O de maior kVA prevalecerá, o outro
 * tem o kVA zerado para ser ignorado na exportação para CSV.
 * Quanto menor o raio do círculo, maior o número de transformadores.
 */
    for(auto j = 0; j < vec_postes.size() - 1; j++) {
      for(auto k = j+1; k < vec_postes.size(); k++) {
        squared_dist = // distância ^ 2
          pow(vec_postes[j].x - vec_postes[k].x, 2) + pow(vec_postes[j].y - vec_postes[k].y, 2);

        if(squared_dist < 15625) // Só considerados postos a menos de 125 m um do outro
          relevo[relevo[k] > relevo[j]?j:k] = 0;
      } // for(auto k = j+1; k < vec_postes.size(); k++)
    } // for(auto j = 0; j < vec_postes.size() - 1; j++)


    CDXF csvdxf("TesteInterpol.csv");
    csvdxf.layer("Interpol", (Word)CDXF::cyan);

    for(auto j = 0; j < vec_postes.size(); j++) {
      if(relevo[j] > 0) {
//        str.sprintf("%d;%.6f;%.6f;%.6f", j, vec_postes[j].x, vec_postes[j].y, relevo[j]);
//        qDebug().noquote() << str;
        csvdxf.circle("Interpol", vec_postes[j].x, vec_postes[j].y, 15., (Word)CDXF::by_layer);
      } // if(eleito[k] == true)
    } // for(auto j = 0; j < vec_postes.size(); j++)

    delete[] relevo;
  } // if(is.is_open()) // Postes.csv

  close();
} // MainWindow::interpolacao2()
//------------------------------------- ------------------------------------------------------------

void MainWindow::menorCaminho2()
{
  std::vector<SPoste> vec_postes;

  carregarPostes(vec_postes, 9999);
  gbPtr1 = &vec_postes;

  carregarArcos();

  int idPostos[]{ 31,65,44,74,90,132,146,168,179,185,194,206,215,231,233 };

  int sz = sizeof(idPostos)/sizeof(int);

  for(auto x = 0; x < sz - 1; x++) {
    for(auto y = x + 1; y < sz; y++) {
      float dist = menorCaminho2(idPostos[x], idPostos[y]);
//      if(dist > DIST_MAX)
      if(dist < 200)
        qDebug().noquote().nospace() << idPostos[x] << ";" << idPostos[y] << ";" << dist;
    }
  }

  close();
}
//------------------------------------- ------------------------------------------------------------

void MainWindow::locarTrafos()
{
  std::vector<SPoste> vec_postes;

  carregarPostes(vec_postes, 9999);
  gbPtr1 = &vec_postes;

  carregarArcos();

/* Vetor para guardar o par de nós dos arcos. A cada arco correspondem duas entradas no vetor.
 * As entradas com índices parer (0, 2, 4, ...) fazem referência ao primeiro ponto dos arcos.
 * As entradas com índices ímpares (1, 3, 5, ...) referenciam o segundo ponto dos arcos.
 * Por exemplo, para um arco de n1 a n2:
 *
 * vec_sec.push_back(n1); // primeiro ponto do arco
 * vec_sec.push_back(n2); // segundo ponto do arco
 */
  std::vector<int> vec_sec;
  gbPtr2 = &vec_sec;

//  short postos[]{ 28, 33, 65, 111, 151, 164, 171, 179, 185, 199, 205, 226, 234 };
//  int idPostos[]{ 28,34,59,72,111,143,159,164,171,187,198,207,221 };
  int idPostos[]{ 31,65,44,74,90,215,132,146,168,179,185,194,206,231,233 };
//  int idPostos[]{ 26,31,28,44,30,90,61,146,63,179,65,66,194,55,215 };

  int sz = sizeof(idPostos)/sizeof(int);

{
  QDebug deb = qDebug().noquote().nospace();
  deb << "L";
  for(auto i = 0; i < sz; i++)
    deb << ";" << idPostos[i];
}

  Word nAlteracoes;
  QElapsedTimer timer;
  Arco *ptrArcos = arcos.getData();
  timer.start();

  do {
    SPoste &pole = vec_postes[0];
qDebug().noquote().nospace() << "1. " << pole.id << ";" << pole.posto << ";" << pole.kva;
    for(auto k = 0; k < vec_postes.size(); k++)
      vec_postes[k].dist2Posto = 9999;

    for(auto k = 0; k < arcos.size(); k++) {
      Arco &arco = ptrArcos[k];
      arco.visitado = false;
    }

    nAlteracoes = 0;
    vec_sec.clear();
    modifiedKmeans(idPostos, sz);
qDebug().noquote().nospace() << "2. " << pole.id << ";" << pole.posto << ";" << pole.kva;
    std::vector<int> postes;

    criarCsvDxf(idPostos, sz, vec_postes, vec_sec);

    for(auto k = 0; k < sz; k++) { // Cada posto
qDebug().noquote().nospace() << "Posto: " << idPostos[k];

      for(auto p = 0; p < vec_postes.size(); p++) {
// Preencher o vetor postes com os id's do postes do posto idPostos[k]
        SPoste poste = vec_postes[p];
        if(poste.posto == idPostos[k]) {
//qDebug().noquote().nospace() << poste.id;
          postes.push_back(poste.id);
        }
      }

      if(postes.size() > 1) {
        int champion = centroDeCarga(postes);
  qDebug().noquote().nospace() << "3. " << pole.id << ";" << pole.posto << ";" << pole.kva;
  qDebug().noquote() << "champion: " << champion;
        if(champion != idPostos[k]) {
//          for(auto l = 0; l < vec_postes.size(); l++) {
//            SPoste &pst = vec_postes[l];
//            if(pst.posto == idPostos[k])
//              pst.posto = champion;
//          }
          nAlteracoes++;
          idPostos[k] = champion;
        }
      } // if(postes.size() > 1)
      postes.clear();
    } // for(auto k = 0; k < sz; k++)

    {
      QDebug deb = qDebug().noquote().nospace();
      deb << "L";
      for(auto i = 0; i < sz; i++)
        deb << ";" << idPostos[i];
    }

//qDebug() << "nAlteracoes: " << nAlteracoes;
  } while(nAlteracoes);

  for(auto k = 0; k < sz; k++)
    qDebug().noquote().nospace() << "Posto: " << idPostos[k];


  for(auto p = 0; p < vec_postes.size(); p++) {
    SPoste poste = vec_postes[p];
    qDebug().noquote().nospace() << poste.id << ";" << poste.posto << ";" << poste.kva;
  }

  qDebug() << "The locarTrafos() process took" << timer.elapsed()/1000. << "seconds";

  {
    QDebug deb = qDebug().noquote().nospace();
    deb << "L";
    for(auto i = 0; i < sz; i++)
      deb << ";" << idPostos[i];
  }

  criarCsvDxf(idPostos, sz, vec_postes, vec_sec);

  close();
} // void MainWindow::locarTrafos()
//------------------------------------- ------------------------------------------------------------

/**
 * Criar o arquivo CSV para conversão para DXF pelo programa <b>Csv2Dxf.sh</b>.
 * O arquivo criado contém os postos de transformação representados por círculos
 * e os circuitos secundários em diferentes cores.
 *
 * @param idPostos Array com os identificadores dos postes nos quais estão os postos.
 * @param vec_postes Vetor com os identificadores dos postes.
 * @param vec_sec Vetor com os identificadores dos postes extremos dos arcos.
 */
void MainWindow::criarCsvDxf(int idPostos[], int sz, std::vector<SPoste> vec_postes, std::vector<int> vec_sec)
{
  CDXF csvdxf((char*)"RedeSecundária.csv");
  csvdxf.layer((char*)"Secundária", (Word)CDXF::red);
  csvdxf.layer((char*)"Postos", (Word)CDXF::black_white);

  csvdxf.layer((char*)"0", (Word)CDXF::black_white, CDXF::locked);
  csvdxf.line((char*)"0", 314587, 7454120, 314587, 7454140, CDXF::red);
  csvdxf.line((char*)"0", 314577, 7454130, 314597, 7454130, CDXF::red);
  csvdxf.circle((char*)"0", 314587, 7454130, 10, CDXF::red);

  float x1, y1, x2, y2;

  for(auto p = 0; p < sz; p++) {
    SPoste &poste = vec_postes.at(idPostos[p] - 1);
    csvdxf.circle((char*)"Postos", poste.x, poste.y, 5.0);
  }

  for(auto p = 0; p < vec_sec.size();) {
    Word cor;
    SPoste p1 = vec_postes.at(vec_sec.at(p++) - 1);

    int id_posto1 = p1.posto;
    x1 = p1.x;
    y1 = p1.y;

    SPoste p2 = vec_postes.at(vec_sec.at(p++) - 1);
    x2 = p2.x;
    y2 = p2.y;

    for(auto k = 0; k < sz; k++)
      if(idPostos[k] == id_posto1) {
        cor = k;
        break;
      }

    if(p1.posto == p2.posto)
      csvdxf.line("Secundária", x1, y1, x2, y2, cor);
  }
}
//------------------------------------- ------------------------------------------------------------

/**
 * Atribuir a cada posto os postes mais "próximos" a ele. O procedimento executa um algorítimo
 * K-Means modificado, no qual, ao invés de distâncias euclidianas, são usadas as distâncias ao
 * longo da rede através dos arcos.
 *
 * @param idPostos Relação dos identificadores dos postes nos quais estão localizados
 * os postos de transformação.
 * @param sz Número de postos.
 * @see menorCaminho1(int, int, int, float, float)
 */
void MainWindow::modifiedKmeans(int idPostos[], int sz)
{
  QString str;
  std::vector<SPoste> *vec_postes = (std::vector<SPoste>*)gbPtr1;
  qDebug().noquote().nospace() << "modifiedKmeans";

  for(auto k = 0; k < sz; k++) {
    SPoste &poste = vec_postes->at(idPostos[k] - 1);
    poste.posto = idPostos[k];
    float kvaTotal = menorCaminho1(-1, idPostos[k], idPostos[k]);
    qDebug().noquote().nospace() << idPostos[k] << ";" << kvaTotal;
  }
} // void MainWindow::modifiedKmeans()
//------------------------------------- ------------------------------------------------------------

/**
 * Função recursiva para atribuir a determinado posto de transformação os postes mais próximos
 * a ele. É invocada uma vez para cada posto. Se um poste já foi atribuido a outro posto, porém
 * está mais próximo do corrente, é sequestraedo e muda de posto.
 * @param anterior Identificador do poste anterior ao corrente na rede.
 * @param id_posto Identificador do poste do posto para o qual está sendo feita a clusterização.
 * @param current Identificador do poste corrente, o que está sendo tratado.
 * @param distancia Distância do poste corrente ao posto.
 */
float MainWindow::menorCaminho1(int anterior, int id_posto,
  int current, float distancia /*0.0*/, float kvaTotal /*0.0*/)
{
  std::vector<SPoste> *vec_postes = (std::vector<SPoste>*)gbPtr1;
  SPoste poste_cur = vec_postes->at(current - 1);
//  SPoste p2 = vec_postes->at(id_posto - 1);

//qDebug().noquote() << "current: " << current;
if(current == 160) // || current == 78)
  breakpoint = 0;

  int n2;
  std::vector<Arco*> lista = findArcos(current);
  std::vector<int> *vec_sec = (std::vector<int>*)gbPtr2;
//  float deltaAng;

  for(auto j = 0; j < lista.size(); j++) {
    Arco *arco = lista[j];
//    if(arco->p1 == poste_cur.id) n2 = arco->p2;
//    else n2 = arco->p1;  // inverter
    n2 = arco->p1 == poste_cur.id?arco->p2:arco->p1;

    if(n2 == anterior) // está querendo voltar, não pode
      continue;
    float d = distancia + arco->metros;
    SPoste &next = vec_postes->at(n2 - 1);
    float kva = kvaTotal + next.kva;

    if((d < LIMITE_ND22 && d < next.dist2Posto && kva <= 45) || poste_cur.atribs & atbFLYING_TAP) {
//      ((next.atribs & atbPOSTE_FT) == 0 || next.posto == poste_cur.posto)) {
//      || poste_cur.atribs & atbFLYING_TAP) {
//      (next.atribs & atbPOSTE_FT) == 0) || poste_cur.atribs & atbFLYING_TAP
//      || poste_cur.posto == id_posto) {

      next.posto = id_posto;
      next.dist2Posto = d;
      kvaTotal = kva;

      if(arco->visitado == false) { // Só para não desenhar 2 vezes
        arco->visitado = true;
        vec_sec->push_back(current);
        vec_sec->push_back(n2);
      }

      kvaTotal = menorCaminho1(current, id_posto, n2, d, kvaTotal);
breakpoint = 0;
    } // if((d < LIMITE_ND22 && d < next.dist2Posto && kva <= 45) || poste_cur.atribs & atbFLYING_TAP)

    else break;
  } // for(auto j = 0; j < lista.size(); j++)

  return kvaTotal;
} // menorCaminho1(int, int, int, float, float)
//------------------------------------- ------------------------------------------------------------

/**
 * Função recursiva para buscar o menor caminho entre um ponto de partida e um ponto alvo.
 * A busca é interrompida quando ultrapassar um limite máximo (DIST_MAX) ou quando ultrapassar
 * a distância já obtida por outro caminho. O ponto de partida deve existir, senão ocorre
 * exceção.
 *
 * @param id_current Identificador do ponto atual. Usar o ponto de partida.
 * @param id_target Ponto alvo.
 * @param anterior Ponto anterior ao corrente. Não prover, iniciado com -1.
 * @param distancia Distância ao ponto anterior. Não prover.
 * @return Distância em metros do ponto de partida ao alvo se o ponto é encontrado e
 * INFINITE_FLOAT se o alvo não existe ou estiver fora de alcance (distância > DIST_MAX).
 */
float MainWindow::menorCaminho2(int id_current, int id_target, int anterior, float distancia /*0*/)
{
  static float menorDistancia = INFINITE_FLOAT;
  std::vector<SPoste> *vec_postes = (std::vector<SPoste>*)gbPtr1;
  SPoste current = vec_postes->at(id_current - 1);

  int n2;
  std::vector<Arco*> lista = findArcos(id_current);

  if(anterior == -1)
    menorDistancia = INFINITE_FLOAT;

  for(auto j = 0; j < lista.size(); j++) {
    Arco *arco = lista[j];
    n2 = arco->p1 == current.id?arco->p2:arco->p1;

    if(n2 == anterior)
// está tentando voltar voltar// ou é o primeiro ponto
      continue;

// distancia do ponto de partida até n2
    float dist = distancia + arco->metros;

    if(dist > DIST_MAX) // Deve desistir deste caminho
      return -1;

    if(n2 == id_target) { // alcançou o alvo
      if(dist < menorDistancia)
        menorDistancia = dist;
      return menorDistancia;
    }

    menorCaminho2(n2, id_target, id_current, dist);
//breakpoint = 0;
  } // for(auto j = 0; j < lista.size(); j++)

  return menorDistancia;
} // menorCaminho2(int, int, int, float)
//------------------------------------- ------------------------------------------------------------

void MainWindow::interpolacao3(int anterior, int current,
  int id_posto, float *relevo, float distancia /*0*/)
{
  std::vector<SPoste> *vec_postes = (std::vector<SPoste>*)gbPtr1;
  SPoste &refP1 = vec_postes->at(current - 1);
//  SPoste &posto = vec_postes->at(id_posto - 1);

  int p1, p2;
  Arco *arco;
  QString qString;

//qDebug().noquote().nospace() << qString.sprintf("%9.2f,%10.2f", refP1.x, refP1.y);
//  std::vector<int> *vec_sec = (std::vector<int>*)gbPtr2;

  std::vector<Arco*> lista = findArcos(current);

/*
  if(lista.size() == 1) {
// Fim da linha
    arco = lista[0];
    p2 = arco->p1 == current?arco->p2:arco->p1;
//qDebug().noquote().nospace() << current << ", " <<  p2;

    if(p2 == anterior)
breakpoint = 1;
    else {
//      SPoste &refP2 = vec_postes->at(p2 - 1);
      float d = distancia + arco->metros;

      if(d <= LIMITE_ND22) { // Item 6.1.4.1 ND 22 Elektro
        relevo[id_posto - 1] += refP1.kva/d;
//        refP1.posto = id_posto;
qDebug().noquote().nospace() << qString.sprintf("A;%3d;%3d", current, p2);
//        vec_sec->push_back(current);
//        vec_sec->push_back(p2);
//        kvaTotal += refP2.kva;
//        distancia = d;
//qDebug().noquote().nospace() << qString.sprintf("%3d;%6.2f;%6.2f", p2, distancia, kvaTotal);
      }
    } // if(p2 != anterior)
  } // if(lista.size() == 1)
*/

//  else { // lista.size() > 1
    for(int j = 0; j < lista.size(); j++) {
      arco = lista[j];
      if(arco->p1 == current) { p1 = arco->p1; p2 = arco->p2; }
      else { p1 = arco->p2; p2 = arco->p1; } // inverter

      if(p2 == anterior && anterior != -1) // está querendo voltar, não pode
        continue;

//      SPoste &refP2 = vec_postes->at(p2 - 1);
      float d = distancia + arco->metros;

//      if(d <= LIMITE_ND22 // Item 6.1.4.1 ND 22 Elektro
      if(d <= 200 // Item 6.1.4.1 ND 22 Elektro
        || (refP1.atribs & (atbFLYING_TAP | atbPOSTE_TOPO)) != 0) { // É flying-tap ou poste de topo
// Se o ponto é flying-tap, LIMITE_ND22 pode ser ultrapassado para que não fique incompleto.
// Se o ponto é poste de topo, contrói pelo menos um vão de cada lado, isto é, se veio da rua
// que termina na do poste de topo, contrói pelo menos um vão de cada lado nessa rua, mesmo que
// LIMITE_ND22 seja ultrapassado.
//        vec_sec->push_back(current);
//        vec_sec->push_back(p2);
//        kvaTotal += refP2.kva;
/*
        if(d < refP1.dist2Posto) {
          if(refP1.posto != id_posto) {
//            SPoste &posto0 = vec_postes->at(refP1.posto - 1);
            if(refP1.posto > 0)
              relevo[refP1.posto] -= refP1.kva;
            refP1.posto = id_posto;
          }
//          refP1.dist2Posto = d + 0.5;
          relevo[id_posto] = refP1.kva/d;
        }
*/
        relevo[id_posto - 1] += refP1.kva/d;
        interpolacao3(current, p2, id_posto, relevo, d);
breakpoint = 1;
      }
    } // for(int j = 0; j < lista.size(); j++)
//  } // else (lista.size() > 1)

//  return distancia;
} // interpolacao3(int, int, int, float&, float)
//--------------------------------------------------------------------------------------------------

/**
 * Carregar o arquivo RD2000PSF.CSV com a rede secundária do Prodadis da região do Parque
 * São Francisco, Itatiba. Os dados são salvos no arquivo <b>Rede41408.csv</b> para posterior
 * conversão para DXF pela aplicação <b>Csv2Dxf.sh</b>.
 *
 * Para usar esta, mudar a função em <b>on_btFuncao_clicked()</b> para <b>FN_41408</b>.
 */
void MainWindow::RD2000()
{
  QString str;
  char buffer[128];
  std::ifstream is;

/* RD2000PSF.CSV
 * Não pode haver linha com títulos de colunas, a não ser como comentário.
 * USAR PONTO DECIMAL '.'
 *
 * Arquivo deve estar ordenado por pontoId.
 * Arauivo chama Postes.csv, mas tam flying-taps também.
 *
 * // Rede secundária Jd. São Francisco - Itatiba
 * // Postox;Postoy;Dex;Dey;Parax;Paray
 * 313783.0;7454550.0;313783.0;7454550.0;313743.7;7454552.1
 * 313783.0;7454550.0;313783.0;7454550.0;313820.0;7454555.9
 * 313867.5;7454106.5;313867.5;7454106.5;313845.4;7454135.5
 * |        |         |        |         |        |
 * |        |         |        |         |        Pary
 * |        |         |        |         Parax
 * |        |         |        Dey
 * |        |         Dex
 * |        Postoy
 * Postox
 */
  is.open("/home/john/Atalhos/Qt/QuedaTensao/Dados/RD2000PSF.CSV");
  if(!is.is_open()) {
    std::cout << "File RD2000PSF.csv not found." << endl;
    return;
  }

  CDXF csvdxf((char*)"Rede41408.csv");
  csvdxf.layer((char*)"RD2000", (Word)CDXF::red);
  csvdxf.layer((char*)"RD4000", (Word)CDXF::black_white);

  csvdxf.layer((char*)"0", (Word)CDXF::black_white, CDXF::locked);
  csvdxf.line((char*)"0", 314587, 7454120, 314587, 7454140, CDXF::red);
  csvdxf.line((char*)"0", 314577, 7454130, 314597, 7454130, CDXF::red);
  csvdxf.circle((char*)"0", 314587, 7454130, 10, CDXF::red);

  float x = 0, y = 0, dex, dey, parax, paray, postox, postoy;

  Word cor = 0;

  while(!is.eof()) {
    is.getline(buffer, 128);
    if(buffer[0] == '/' && buffer[1] == '/') // comentário
      continue;
    str = buffer;
    if(str.isEmpty())
      continue;

    QStringList stringList = str.split(";");
    postox = stringList.at(0).toFloat();
    postoy = stringList.at(1).toFloat();

    if(postox != x || postoy != y) {
      cor++;
      if(cor == 17)
        cor = 1;
      x = postox;
      y = postoy;
    }
    dex = stringList.at(2).toFloat();
    dey = stringList.at(3).toFloat();
    parax = stringList.at(4).toFloat();
    paray = stringList.at(5).toFloat();
    csvdxf.line("RD2000", dex, dey, parax, paray, cor);
  }
}
//--------------------------------------------------------------------------------------------------

/**
 * Carregar o arquivo RD4000PSF.CSV com os transformadores do Prodadis da região do Parque
 * São Francisco, Itatiba. Os dados são adicionados ao arquivo Rede41408.csv criado pela
 * função <b>RD2000()</b> para posterior conversão para DXF pela aplicação <b>Csv2Dxf.sh</b>.
 *
 * Para usar esta, mudar a função em <b>on_btFuncao_clicked()</b> para <b>FN_41408</b>.
 */
void MainWindow::RD4000()
{
  QString str;
  char buffer[128];
  std::ifstream is;

/* RD4000PSF.CSV
 * Não pode haver linha com títulos de colunas, a não ser como comentário.
 * USAR PONTO DECIMAL '.'
 *
 * Arquivo deve estar ordenado por pontoId.
 * Arauivo chama Postes.csv, mas tam flying-taps também.
 *
 * // Transformadores Jd. São Francisco - Itatiba
 * // Postox;Postoy
 * 313783.0;7454550.0
 * 313783.0;7454550.0
 * 313867.5;7454106.5
 * |        |
 * |        Postoy
 * Postox
 */
  is.open("/home/john/Atalhos/Qt/QuedaTensao/Dados/RD4000PSF.CSV");
  if(!is.is_open()) {
    std::cout << "File RD4000PSF.csv not found." << endl;
    return;
  }

  float postox, postoy;
  CDXF csvdxf2((char*)"Rede41408.csv", 'a');

  Word cor = 0;

  while(!is.eof()) {
    is.getline(buffer, 128);
    if(buffer[0] == '/' && buffer[1] == '/') // comentário
      continue;
    str = buffer;
    if(str.isEmpty())
      continue;

    QStringList stringList = str.split(";");
    postox = stringList.at(0).toFloat();
    postoy = stringList.at(1).toFloat();
    csvdxf2.circle("RD4000", postox, postoy, 5.0);
  }
}
//--------------------------------------------------------------------------------------------------

void MainWindow::on_btFuncao_clicked()
{
  lop();
  close();
  return;
/*
FN_INTERPOL1
FN_INTERPOL2
FN_MODIF_KMEANS
FN_KMEANS
FN_CENTRO_CARGA
*/

// Ao alterar aqui, alterar também o ToolTip em MainWindow(QWidget*)

//  switch(FN_MODIF_KMEANS) {
//  switch(FN_CENTRO_CARGA) {
//  switch(FN_KMEANS) {
//  switch(FN_41408) {
//  switch(FN_MENOR_CAMINHO) {
  switch(FN_LOCAR_TRAFOS) {

    case FN_INTERPOL1:
      interpolacao1();
      break;

    case FN_INTERPOL2:
      interpolacao2();
      break;

//    case FN_MODIF_KMEANS:
//      modifiedKmeans();
//      break;

    case FN_CENTRO_CARGA:
      centroDeCarga();
      break;

    case FN_LOCAR_TRAFOS:
      locarTrafos();
      break;

    case FN_41408:
      RD2000();
      RD4000();
      close();
      break;

    case FN_MENOR_CAMINHO:
      menorCaminho2();
      break;

#ifdef KMEANS
    case FN_KMEANS:
      QElapsedTimer timer;
      timer.start();
    //  for(int i = 0; i < 1000; i++)
        kmeans1();
      qDebug() << "The kmeans process took" << timer.elapsed()/1000. << "seconds";
      break;
#endif
  }
}
//--------------------------------------------------------------------------------------------------
