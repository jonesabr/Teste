#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSqlDatabase>
#include <vector>
#include <algorithm>    // std::sort
#include <unordered_map>
#include "/home/john/Atalhos/Qt/QuedaTensao/cindice.h"
#include "/home/john/Atalhos/Qt/QuedaTensao/cdxf.h"


typedef unsigned char Byte;

//#define KMEANS
#define FN_INTERPOL1      1
#define FN_INTERPOL2      2
#define FN_MODIF_KMEANS   3
#define FN_KMEANS         4
#define FN_CENTRO_CARGA   5
#define FN_LOCAR_TRAFOS   6
#define FN_41408          7
#define FN_MENOR_CAMINHO  8
#define INFINITE_FLOAT 3.4E+38

typedef unsigned short Word;

#ifdef KMEANS
#define NUM_CLUSTERS 16
#endif

namespace Ui {
  class MainWindow;
}
//--------------------------------------------------------------------------------------------------

struct Ponto
{
/// Identificador do ponto.
  int id;
/// Coordenadas do ponto.
  float x, y;
/// Tipo do ponto  (1: poste, 3: FT)
  char tipo;
  float kva_proprio, kva_acumulado;
/// Indica se o ponto já foi visitado.
  bool visitado;
/// distância em metros do trafo até o ponto
//  float dist_acumulada;
/// Queda de tensão no ponto em %
  float qt;

//  bool operator<(const Ponto& p) const { return (this->id < p.id); }

  Ponto() {}
  Ponto(int id, int x, int y, char tipo, float kva_proprio = 0) :
    id(id), x(x), y(y), tipo(tipo), kva_proprio(kva_proprio), visitado(false) {}

//  void setVisited(bool v) { visitado = v; }
};
//--------------------------------------------------------------------------------------------------

/**
 * Estrutura para uso nas funções <b>centroDeCarga()</b> e
 * <b>centroDeCarga(int, int, int, float = 0)</b>
 */
struct SPoste {
  int id;           // Id do poste
  double x, y;      // coordenadas do poste (por enquanto só para debug)
  int posto;        // Id do posto (0 se nenhum)
//  float prodDistKva; // produto (metros até o transformador x kva próprio)
  float dist2Posto;
//  float kvaxmetros;
//  float kva_total;  Soma dos kvas dos postes
//    short ang;
  Byte atribs;      // atributos; ver CTabPostes.h no projeto LocarPostes3.
  float kva;        // carga no poste

  SPoste(int id = 0) : id(id) {}
//  bool operator<(const SPoste &outra) const { return (id < outra.id); }
//  bool operator<(SPoste &outra) { return (id < outra.id); }
  bool operator < (const SPoste &rhs) const { return id < rhs.id; }
};
//--------------------------------------------------------------------------------------------------

struct Arco
{
  int id, p1, p2;
  float metros;
  float qtKva100m; // QT em %/(kva*100 m)
  bool visitado;

//  bool operator<(const Arco& a) const { return (this->p1 < a.p1); }

  Arco(int p1 = -1) : p1(p1) {}
  Arco(int id, int p1, int p2, float m, float qt) :
    id(id), p1(p1), p2(p2), metros(m), qtKva100m(qt) {}
};
//--------------------------------------------------------------------------------------------------

/**
 * Ver <b>Índice de Arcos</b> em <b>Locação de postes.odt</b>. Índice associando o nó de cada
 * uma das extremidades de um arco ao id referente ao mesmo no objeto CIndice<Arco> arcos.
 * A cada arco correspondem duas entradas (nó, id) no objeto <b>SArcosIndex SArcosIndex</b>
 * instanciado desta, uma com nó 1 e outra para o nó 2. Assim, por qualquer das extremidades
 * que se busque um arco, sempre será encontrado. A inserção dos pares (nó, id) em
 * <b>SArcosIndex SArcosIndex</b> é feita em ordem crescente de nó.
 */
struct SArcosIndexPar {
  int noh, arcoId;
  SArcosIndexPar(int noh, int id = -1) : noh(noh), arcoId(id) {}
  bool operator==(const SArcosIndexPar outro) { return (outro.noh == noh); }
};
//--------------------------------------------------------------------------------------------------

struct SArcosIndex
{
private:
///
  int curIdx;

/// vetor de estruturas SArcosIndexPar.
  std::vector<SArcosIndexPar> pares;

public:
  void clear() { pares.clear(); }

/**
 * Retornar o identificador do nó na posição <b>idx</b> se <b>idx</b> >= 0, ou então
 * o na posição do atributo <b>curIdx</b>.
 * @param idx Índice da posição buscada.
 * @return Nó buscado
 */
  int get_noh(int idx = -1) {
    if(idx >= 0) curIdx = idx;
    return pares[curIdx].noh;
  }

/**
 * Retornar o <b>identificador do arco</b> na posição indicada pelo parâmetro passado ou na
 * seguinte à corrente.
 * @param idx Índice da posição buscada.
 * @return <b>Iidentificador do arco</b> na posição idx (vector_id[idx]) se idx >= 0 ou na
 * posição seguinte à corrente (vector_id[curIdx + 1]), desde que o identificador do nó na
 * seguinte seja igual ao da corrente (vector_noh[curIdx + 1] == vector_noh[curIdx]).
 * Senão retorna -1.
 */
   int get_id(int idx = -1) {
    if(idx >= 0) curIdx = idx;
    else {
      if(pares[curIdx++].noh != pares[curIdx].noh)
        return -1;
    }
    return pares[curIdx].arcoId;
  }
  void set_id(int idx, int id) { pares[idx].arcoId = id; }
  int size() { return pares.size(); }
/**
 * Inserir um par (noh, id).
 * @param noh
 * @param id
 */

  void push_back(SArcosIndexPar par) { pares.push_back(par); }
  void push_back(int noh, int id) { pares.push_back(SArcosIndexPar(noh, id)); }

/**
 * Buscar o índice (base 0) da primeira entrada onde noh é igual ao parâmetro passado.
 * @param noh Identificador do nó buscado.
 * @return Índice da primeira entrada cujo identificador do nó == noh, se encontrada,
 * senão retorna -1.
 */
  int find_noh(int noh)
  {
    SArcosIndexPar par = SArcosIndexPar(noh);
    std::vector<SArcosIndexPar>::iterator it = std::find(pares.begin(), pares.end(), par);
    if(it != pares.end()) {
      curIdx = std::distance(pares.begin(), it);
      return curIdx;
    }

    return -1;
  }
//--------------------------------------------------------------------------------------------------

  static bool compara_pares(SArcosIndexPar par1, SArcosIndexPar par2) {
    int noh1 = par1.noh;
    int noh2 = par2.noh;
    return (noh1 < noh2);
  }
//--------------------------------------------------------------------------------------------------

  void sort_pares() { std::sort(pares.begin(), pares.end(), SArcosIndex::compara_pares); }
}; // struct SArcosIndex
//--------------------------------------------------------------------------------------------------

//                                          KMEANS [
#ifdef KMEANS
struct SPontoCarga
{
  int id;
  int cluster;
  float x, y;
  bool visitado;
  double squaredDistAcum;
};
//--------------------------------------------------------------------------------------------------

struct SCluster
{
  int cargaId;
  float x, y;
  int ncargas;
};
//--------------------------------------------------------------------------------------------------

struct ArcoKMeans
{
  int p1, p2;
  ArcoKMeans(int p1, int p2) : p1(p1), p2(p2) {}
};
#endif
//                                          KMEANS ]
//--------------------------------------------------------------------------------------------------

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = 0);
  ~MainWindow();

private slots:
//  void on_btImportCSV_clicked();
//  void on_btPopulate_clicked();
//  void on_btCalcular_clicked();
  void on_pbLocaTrafos1_clicked();
#ifdef KMEANS
  void on_btKMeans_clicked();
//  void on_pbLocaTrafos2_clicked();
#endif

  void on_btFuncao_clicked();

private:
//  int funcao;
  Ui::MainWindow *ui;
  void interpolacao1();
  void interpolacao2();
  void lop();
  void locarTrafos();
  void modifiedKmeans(int[], int);
  void interpolacao3(int, int, int, float*, float = 0.0);
  float menorCaminho1(int, int, int, float = 0.0, float = 0.0);
  void menorCaminho2();
  float menorCaminho2(int, int, int = -1, float = 0);

  void centroDeCarga();
  int centroDeCarga(std::vector<int>);
  float centroDeCarga(int, int, int, float&, float = 0.0);
  void carregarPostes(std::vector<SPoste>&, float = 0);
  void carregarArcos();
  void criarCsvDxf(int[], int, std::vector<SPoste>, std::vector<int>);

  void RD2000();
  void RD4000();

//***************************************** KMEANS [
#ifdef KMEANS
  int ncargas;
  bool houveTroca;
  SPontoCarga cargas[260];
  SCluster clusters[NUM_CLUSTERS];
  CIndice<ArcoKMeans> arcos2;
  std::unordered_multimap<int, int> arcosVisitados;

  void kmeans1();
  int kmeans2();

  bool percorrer(SPontoCarga&, int, int = -1);
#endif
//***************************************** KMEANS ]

  int nPontos;
  float soma;
  Ponto *pontos;
  float kva_total;
  SArcosIndex arcosIndex;
  QSqlDatabase db;
  CIndice<Arco> arcos;
  std::vector<Arco*> findArcos(int);

//  void criarArcos();
  Ponto* findPonto(int);
//  float acumularCarga(int = 0);
//  void calcQT(int);

  void locarTrafo(int);
  float locarTrafo2(int, float = 0);
  void locarTrafo3(int, int, float, float = 0);

  float kva_ramo1, kva_ramo2;
};

#endif // MAINWINDOW_H
