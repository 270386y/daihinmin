/*
動的に評価値を求め，柔軟性に富んだプログラムを目指した．
自身の失敗や成功を記憶し，将来に役立てる．
又，本プログラムを作成するにあたり，田頭幸三様の大貧民プログラムkou2を使わせていただいた．ここに感謝の意を表す．
*/
//switch....序盤の試合で戦術毎の勝率を計算して、勝率の高い戦術を後半に使用する

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> //乱数使用のため追加
#include <unistd.h>

#include "connection.h"
#include "daihinmin.h"

#define TUYOI_LIMIT_INIT 12
#define TUYOI_LIMIT_2 8

const int g_logging = 0; 

struct state_type state;
struct seatsort_state ss;

void shuffle(int array[], int size)
{
  int i = size;
  srand((unsigned)time(NULL));
  while (i > 1) {
    int j = rand() % i;
    i--;
    int t = array[i];
    array[i] = array[j];
    array[j] = t;
  }
}

int main(int argc, char *argv[])
{
  int my_playernum;                //プレイヤー番号を記憶する
  int whole_gameend_flag = 0;      //全ゲームが終了したか否かを判別する変数
  int one_gameend_flag = 0;        //1ゲームが終わったか否かを判別する変数
  int accept_flag = 0;             //提出したカードが受理されたかを判別する変数
  int game_count = 0;              //ゲームの回数を記憶する
  int own_cards_buf[8][15];        //手札のカードテーブルをおさめる変数
  int own_cards[8][15];            //操作用の手札のテーブル
  int ba_cards_buf[8][15];         //場に出たカードテーブルを納める
  int ba_cards[8][15];             //操作用の場の札のテーブル
  int ba_cards_bef[8][15] = {{0}}; //比較用テーブル

  int last_playernum = 0;        //最後に提出したプレイヤー
  int used_cards[8][15] = {{0}}; //提出後のカード
  int i, j, k;                   //汎用変数
  int search[8][15] = {{0}};     //検索用テーブル
  int max = 0;                   //一番強いカードを記録
  int pass_num = 0;              //自身が提出してからの他プレイヤパス回数
  int t, y;                      //前中後半，手役を格納
  int f;                         //汎用フラグ

  int pattern[13][7] = {{0}};
  //[0]～[10] 組の情報：[0]強さ[1]枚数[2]スート[3]jokerを使った数字[4]強さ評価値(以降x)[5]革命時の強さ評価値(revx)[6]優先評価値(y)
  //     [11] 手札の情報：[0]組数[1]革命可能性の有無[2]xの最小値(1min)[3]xの2番目に小さい値(2min)[4]全体評価値(z)[5]革命時のz(revz)[6]革命時の2min(rev2m)
  //     [12] 提出後の状況：[0]選択した組の強さ評価値(x')[1]選択した組提出時の全体評価値(z')[2]ジョーカーの使用(0:未所持,1:1枚所持,2:階段の一部(patmake),3:使用)
  //     leadでは[12]には提出する組の情報が入る

  int tes[3][4][3] = {{{0}}}; //比重調整パラメータ．t=序盤中盤終盤，y=手役(0:Joker,1:単体,2:ペア,3:階段)，[t][y][0]=手役の強さ，[t][y][1]=役提出時の他プレイヤパス数，[t][y][2]=役の評価値
  int tes_buf[3][4][3] = {{{0}}};

  int seat_num = 0;         //ターンプレイヤの席番号
  int score[5] = {0};       //合計スコア 順番はプレイヤ番号順
  double shouritu[6] = {0}; //戦術ごとの勝率
  int ranks[5] = {0};       //1ゲームの各プレイヤの順位(大富豪0...大貧民4,あがれなかった場合4) 順番はプレイヤ番号順
  int jk1card = 0;          //ジョーカーが1枚で出たとき1
  int win_flag = 0;         //誰かがあがった直後1
  int ba_cut = 0;           //場が流れるとき1
  int gscore[6] = {0};   //直近100試合での各戦術の取得スコア
  int ruikei[6] = {0};   //戦術ごとの累計すこあ
  int shiai[6] = {0};    //100試合での戦術ごとの試合数
  int shiaisuu[6] = {0}; //戦術ごとの累計試合数
  int senjutu = 0;       //戦術番号
  double maxscore = 0;   //100試合ごとに一番高い点数を代入
  int maxsenjutu = 0;    //100試合ごとの一番強い戦術
  int key = 0;           //戦術決定の際に使用
  int tyuusen = 0;       //1~100の重複しない数字を格納
  //int data[100] = {0};//1~100の数字を格納、戦術決定に使用
  double NO[3] = {0};                      //1000試合で1~3番目に高い勝率
  int NOsenjutu[3] = {0};                  //1000試合で1~3番目に高い勝率を持つ戦術
  int values[100];                         ////1~100の数字を格納、戦術決定に使用
  int size = sizeof(values) / sizeof(int); //valuesの長さ
  int init_value = 100;               //初期手札評価値を格納
  int value2 = 100;                   //2回流れたときの手札評価値を格納
  int init_valueJudge = 0;            //手札が「強い」とき0,「弱い」のとき1
  int value2Judge = 0;                //手札が「強い」とき0,「弱い」のとき1
  int nagare_count = 0;               //場が流れた回数を格納
  int keepJudge_cards = 0;            //提出手が温存の対象であれば1,そうでなければ0
  int win_playernum = 0;              //現時点で上がったプレイヤーの数
  int valueflag = 0;                  //2回場が流れたら1
  int top_seatnum = -1;               //大富豪だと思われるプレイヤーの席番号（自分を0として）、いなければ-1 1.28に変更
  int init_handqty[5] = {0};          //各プレイヤの初期手札枚数（席順）
  int nagashi_whether[5][10] = {{0}}; //各場をどのプレイヤが流したか記録する配列
                                      //プレイヤiがj回目に場を流したとき[i][j-1]=1
  int strong[5] = {0};                //強いプレイヤ予測の時に使う各プレイヤの強さ度、値が大きいほど強い
  int strong_max = 0;                 //強さ度の最大値
  int judgeflag = 0;                  //強いプレイヤ予測をしたら1、してないなら0
  int random_judge = 0;               //温存条件に当てはまった時に実際に温存するか決めるための変数
  int samesuit = 0;                   //提出手が階段かどうか判断するときに使う変数 
  int samestrong = 0;                 //提出手が強いペアか判断するときに使う変数
  int kaidanflag = 0;                 //提出手が階段だったら1、温存のときに使う 
  int strongpair_flag = 0;            //提出手が強いペアだったら1、温存の時に使う 
  double correct = 0.0;               //現時点での強いプレイヤ予測の正解率
  double temp = 0.0;                  //correct計算用 
  int use_strength = 0;               //使う手札の強さ 
  int my_strongqty = 0;               //自分の手札の強い手札の枚数 
  int top_ranks[5][5] = {{0}};

  int attack_flag = 0; //senjutu3でtesが変更されたとき

  //debug
  int onzon = 0;

  //murata(senjutu == 0)////////////////////
  int rank_buf = 0; //自分のランクを格納（席順戦略確認用）
  int seat_order_flag = 0; //席順チェックのフラグ
  //座ってる順番を格納する配列[->自分→〇→〇→〇→〇-]
  int seat_order[5] = {0};
  //席順戦略でパスした時ちゃんと流れているか
  int pass_check_lag[2][100000] = {0};
  //場が空の時のクライアントごとの提出履歴
  int onset_flag = 0;
  //-------------------------------------------
  //引数のチェック 引数に従ってサーバアドレス、接続ポート、クライアント名を変更
  checkArg(argc, argv);
  //ゲームに参加
  my_playernum = entryToGame();
  while (whole_gameend_flag == 0) {
    one_gameend_flag = 0;                  //1ゲームが終わった事を示すフラグを初期化
    game_count = startGame(own_cards_buf); //ラウンドを始める 最初のカードを受け取る。
    copyTable(own_cards, own_cards_buf);   //もらったテーブルを操作するためのテーブルにコピー

    //100ゲーム毎にデータ操作等の処理
    if (game_count % 100 == 1) { //階級をリセットするときに，前回の階級を平民にする
      for (i = 0; i < 5; i++) {
        ranks[i] = 2;
        //100試合ごとにをgscore[5],shiai[5]を初期化
        for (i = 0; i < 5; i++) {
          gscore[i] = 0;
          shiai[i] = 0;
        }
      }

      //毎試合ごとの席順チェックのフラグの初期化
      seat_order_flag = 0;

      if (game_count % 500 == 1) { //500ゲーム目で負け越していた場合，動的評価値を一旦リセット
        for (i = 0; i < 5; i++) {
          if (i != my_playernum) {
            if (score[my_playernum] < score[i]) {
              for (j = 0; j < 3; j++) {
                for (k = 0; k < 4; k++) {
                  tes[j][k][2] = 0;
                }
              }
              break;
            }
          }
        }
      }
    }

    //:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
    //使用する戦術の決定

    senjutu = 3;

    /*if (game_count % 100 == 1) {
      for (i = 0; i < 100; i++) {
        values[i] = i;
      }
      shuffle(values, size);
    }
    tyuusen = values[game_count % 100];
   
    if (game_count <= 1000) 
    {
      if (tyuusen < 17) {
        senjutu = 0;
      } else if ((17 <= tyuusen) && (tyuusen < 34)) {
        senjutu = 1;
      } else if ((34 <= tyuusen) && (tyuusen < 51)) {
        senjutu = 2;
      } else if ((51 <= tyuusen) && (tyuusen < 68)) {
        senjutu = 3;
      } else if ((68 <= tyuusen) && (tyuusen < 84)) {
        senjutu = 4;
      } else {
        senjutu = 5;
      }
    }

    else if ((game_count > 1000) && (game_count <= 3000)) 
    {
      if (tyuusen < 28) {
        senjutu = maxsenjutu;
      } else if ((28 <= tyuusen) && (tyuusen < 40)) {
        senjutu = 0;
      } else if ((40 <= tyuusen) && (tyuusen < 52)) {
        senjutu = 1;
      } else if ((52 <= tyuusen) && (tyuusen < 64)) {
        senjutu = 2;
      } else if ((64 <= tyuusen) && (tyuusen < 76)) {
        senjutu = 3;
      } else if ((76 <= tyuusen) && (tyuusen < 88)) {
        senjutu = 4;
      } else {
        senjutu = 5;
      }
    } else {
      if (tyuusen < 50) {
        senjutu = NOsenjutu[0];
      } else if ((50 <= tyuusen) && (tyuusen < 75)) {
        senjutu = NOsenjutu[1];
      } else if ((75 <= tyuusen) && (tyuusen < 100)) {
        senjutu = NOsenjutu[2];
      }
    }*/

    //:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
    //ゲーム開始時にデータ格納と初期化
    for (i = 0; i < 5; i++) { //席順を調べる
      if (own_cards_buf[6][10 + i] == my_playernum) {
        for (j = 0; j < 5; j++) {
          if ((i + j) < 5) {
            ss.seat[j] = own_cards_buf[6][10 + i + j];
          } else {
            ss.seat[j] = own_cards_buf[6][5 + i + j];
          }
        }
      }
    }

    for (i = 0; i < 5; i++) {
      ss.score[i] = score[ss.seat[i]];               //合計スコアセット
      ss.old_rank[i] = ranks[ss.seat[i]];            //前ゲームの階級セット
      ss.hand_qty[i] = own_cards_buf[6][ss.seat[i]]; //初期の手札枚数セット
      ss.pass[i] = 0;                                //パス/あがり状況リセット
    }
    for (i = 0; i < 5; i++) {
      ranks[i] = 4; //順位4にリセット
    }
    clearTable(used_cards);
    clearTable(search);
    jk1card = 0;
    win_flag = 0;
    ba_cut = 1;

    ///カード交換
    if (own_cards[5][0] == 0) { //カード交換時フラグをチェック ==1で正常
      printf("not card-change turn?\n");
      exit(1);
    } else { //テーブルに問題が無ければ実際に交換へ
      if (own_cards[5][1] > 0 && own_cards[5][1] < 100) {
        int change_qty = own_cards[5][1]; //カードの交換枚数
        int select_cards[8][15] = {{0}};  //選んだカードを格納

        //自分が富豪、大富豪であれば不要なカードを選び出す
        /////////////////////////////////////////////////////////////
        //カード交換のアルゴリズムはここに書く
        /////////////////////////////////////////////////////////////
        kou_change(select_cards, own_cards, change_qty, tes);
        /////////////////////////////////////////////////////////////
        //カード交換のアルゴリズム ここまで
        /////////////////////////////////////////////////////////////

        //選んだカードを送信
        sendChangingCards(select_cards);
      } else {
        //自分が平民以下なら、何かする必要はない
      }
    } //カード交換ここまで

    //////////////////////////////////////////////////////////////////////////////
    if (senjutu == 0) {
      //席順戦略有効性の確認用---------------------
      if (game_count >= 1) {
        for (i = 0; i < 5; i++) {
          if (i == my_playernum) {
            rank_buf = state.player_rank[i];
            break;
          }
        }

        //自分がなった階級を格納
        seat_tactics_check[(game_count - 1)][1] = rank_buf;
      }
      //-------------------------------------------
    }

    /////////////////////////////////////////////////////////////////////////////////////
    while (one_gameend_flag == 0) { //1ゲームが終わるまでの繰り返し
      int select_cards[8][15] = {{0}};        //提出用のテーブル
      if (receiveCards(own_cards_buf) == 1) { //カードをown_cards_bufに受け取り
        //場を状態の読み出し
        //自分のターンであるかを確認する
        //自分のターンであればこのブロックが実行される。
        clearCards(select_cards);            //選んだカードのクリア
        copyTable(own_cards, own_cards_buf); //カードテーブルをコピー
        /////////////////////////////////////////////////////////////
        //アルゴリズムここから
        //どのカードを出すかはここにかく
        /////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////
        if (senjutu == 0) {
          //自分と相手の席の関係を格納する--------------------
          if (seat_order_flag == 0) {
            for (i = 0; i < 5; i++) {
              if (own_cards[6][10 + i] == my_playernum) {
                break;
              }
            }

            for (j = 0; j < 5; j++) {
              seat_order[j] = own_cards[6][10 + (i + j) % 5];
            }

            seat_order_flag = 1;

            //前席クライアントの階級格納
            if (game_count >= 1) {
              seat_tactics_check[game_count][2] = state.player_rank[seat_order[4]];
            }
          }
          //--------------------------------------------------
        }
        ////////////////////////////////////////////////////////////////////

        max = 0; //自分以外のカードの強さの最大
        if (state.rev == 0) {
          for (i = 13; i > 0 && max == 0; i--) {
            if (own_cards[0][i] + own_cards[1][i] + own_cards[2][i] + own_cards[3][i] + used_cards[4][i] != 4) { //同じ強さのカードが他の手札にあれば
              max = i;                                                                                           //その強さを記録
            }
          }
        } else {
          for (i = 1; i < 14 && max == 0; i++) {
            if (own_cards[0][i] + own_cards[1][i] + own_cards[2][i] + own_cards[3][i] + used_cards[4][i] != 4) { //同じ強さのカードが他の手札にあれば
              max = i;                                                                                           //その強さを記録
            }
          }
        }
        if (max == 0) {
          if (state.rev == 0) {
            max = 1;
          } else {
            max = 13;
          }
        }

        for (i = 0; i < 13; i++) {
          for (j = 0; j < 6; j++) {
            pattern[i][j] = 0;
          }
        }
        pat_make(pattern, own_cards, max, used_cards, state.joker, tes);

        if(senjutu == 3){
          // ---------------------------------------------------------
          // 常にTOP（1位）を狙うスナイパー戦略
          // ---------------------------------------------------------
          int target_id = -1; 
          int my_rank_now = ss.old_rank[my_playernum]; 

          int target_rank = 0; 

          // 自分が大富豪なら、富豪を狙う
          if (my_rank_now == 0) {
              target_rank = 1;
          }

          // 設定したランクの人を探す
          for(i = 0; i < 5; i++){
              if(ss.old_rank[i] == target_rank){
                  target_id = i;
                  break;
              }
          }

          // デバッグログ（ターゲットが誰になったか確認）
          fprintf(stderr, "[DEBUG] MyRank:%d, TargetID:%d (Rank%d)\n", 
                  my_rank_now, target_id, target_rank);

          // 攻撃判定
          if (state.onset == 0 && last_playernum == target_id && target_id != -1) {
              attack_flag = 1; // フラグON

              for (j = 0; j < 3; j++) {     
                  for (k = 0; k < 4; k++) { 
                       tes[j][k][2] += 100; 
                  }
              }
          }
        }

        if (senjutu == 5) {
          if (init_value == 100) { 
            init_value = pattern[11][4];
            if (init_value <= TUYOI_LIMIT_INIT) {
              init_valueJudge = 0;
            } else if (TUYOI_LIMIT_INIT < init_value) {
              init_valueJudge = 1;
            }
          }
          if (value2 == 100 && valueflag) {
            valueflag = 0; 
            //手札評価値を記録
            value2 = pattern[11][4];
            //手札の強さを判定
            if (value2 <= TUYOI_LIMIT_2) {
              value2Judge = 0;
            } else if (TUYOI_LIMIT_2 < value2) {
              value2Judge = 1;
            }
          }
        }

        //ここからどのカードを出すか判定-----------------------------------------------------------------------

        if (senjutu == 5) {
          keepJudge_cards = random_judge = kaidanflag = strongpair_flag = 0;
          correct = 0.0;
          //現時点での強いプレイヤ予測の正解率を計算 
          for (i = 0; i < 5; i++) {
            temp = 0.0;
            for (j = 0; j < 5; j++) {
              temp += (double)top_ranks[i][j];
            }
            if (temp != 0.0) {
              correct += ((double)top_ranks[i][0] / temp) / 5.0;
            }
          }
        }
        if (state.onset == 1) { //場にカードが無いとき
          copyTable(own_cards, own_cards_buf);
          kou_lead(select_cards, own_cards, max, used_cards, tes); //新しくカードを出すときの判定
        } else {                                                   //すでに場にカードがあるとき
          if (jk1card == 1 && own_cards[0][1] > 0) {               //スペード3
            k = pattern[11][0];                                    //組数を記録
            own_cards[0][1] = 0;
            pat_make(pattern, own_cards, max, used_cards, state.joker, tes);
            if ((last_playernum != own_cards[5][3] && k >= pattern[11][0]) || (last_playernum == own_cards[5][3] && search[4][1] == 1)) { //組数が増えなければ出す
              select_cards[0][1] = 1;
            }
            copyTable(own_cards, own_cards_buf);              //判定後own_cardsを元に戻す
          } else if (state.qty == own_cards[6][my_playernum]) //手札枚数と場の枚数が同じときは、defaultから選択(出せるときは出す)
          {
            if (state.rev == 0) {
              follow(select_cards, own_cards); //通常時の提出用
            } else {
              followRev(select_cards, own_cards); //革命時の提出用
            }
          } else {
            i = 0;
            if (win_flag == 1 && state.sequence == 0) { //誰かがあがった直後,場の組が場を流せる単体・ペアの場合は提出しない
              i = 1;
              if (state.rev == 0) {
                for (j = state.ord + 1; j <= max; j++) {
                  if (4 - (own_cards[0][j] + own_cards[1][j] + own_cards[2][j] + own_cards[3][j] + used_cards[4][j]) >= state.qty) {
                    i = 0;
                    j = 14;
                  }
                }
              } else if (state.rev == 1) {
                for (j = state.ord - 1; j >= max; j--) {
                  if (4 - (own_cards[0][j] + own_cards[1][j] + own_cards[2][j] + own_cards[3][j] + used_cards[4][j]) >= state.qty) {
                    i = 0;
                    j = 0;
                  }
                }
              }
            }
            if (state.sequence == 1) { //階段組の選択
              clearTable(select_cards);
              kou_followsequence(select_cards, own_cards, max, used_cards, tes);
            } else if (i == 0 && state.qty != 5) { //枚数組の選択(5枚の場合は出せないので入らない)
              clearTable(select_cards);
              kou_followgroup(select_cards, own_cards, max, used_cards, tes); //出すカードの判定
            }
          }
          //最後にカードを出したプレイヤーが自分の場合
          if (beEmptyCards(select_cards) == 0 && last_playernum == own_cards[5][3]) { //自分以外パス
            j = pattern[11][3];                                                       //2minを記録
            k = pattern[11][0];                                                       //組数を記録
            cardsDiff(own_cards, select_cards);                                       //提出予定の組をown_cardsから抜く
            pat_make(pattern, own_cards, max, used_cards, state.joker, tes);          //提出後のパターンを作る
            if (pattern[11][3] < 100) {           //提出後2min<100の場合(2min>=100の場合は提出)
              if (j > 70 && j > pattern[11][3]) { //提出後<提出前かつ提出前>70
                clearTable(select_cards);
              }
              //ジョーカー単体は出さない
              if (select_cards[0][0] == 2)
                select_cards[0][0] = 0;
              //強いカードの枚数組は出さない
              if (state.sequence == 0 && state.rev == 0) {
                for (i = max; i <= 13; i++) {
                  select_cards[0][i] = 0;
                  select_cards[1][i] = 0;
                  select_cards[2][i] = 0;
                  select_cards[3][i] = 0;
                }
              }
              if (state.sequence == 0 && state.rev == 1) {
                for (i = max; i >= 1; i--) {
                  select_cards[0][i] = 0;
                  select_cards[1][i] = 0;
                  select_cards[2][i] = 0;
                  select_cards[3][i] = 0;
                }
              }
              //組の数が減らない場合は出さない
              if (k <= pattern[11][0]) {
                clearTable(select_cards);
              }
            }
          }
        } //すでに場にカードがあるときの判定ここまで
        if (beEmptyCards(select_cards) == 0) {
          f = 0;
        }
 
        //強さが「普通」の時
        if (senjutu == 5) {
          if ((value2Judge == 1 || init_valueJudge == 1) && state.rev == 0) {
            my_strongqty = use_strength = 0;
            //自分の手札に強い手札が何枚あるか調べる 
            for (i = 0; i <= 4; i++) {
              for (j = 1; j <= 13; j++) {
                if ((own_cards[i][j] == 1 && j >= 10) || own_cards[4][1] == 2) {
                  my_strongqty++;
                }
                if (i == 0) {
                  if (select_cards[0][j] + select_cards[1][j] + select_cards[2][j] + select_cards[3][j] > 0) {
                    use_strength = j; //提出手の強さを記録
                    //ジョーカー単体の時
                  } else if (select_cards[0][0] == 2) {
                    use_strength = 14; // 2の上
                  }
                }
              }
            }
            //強いプレイヤ判定まで温存判定
            if (nagare_count < 4) {
              if (use_strength >= 11 && win_playernum < 1 && used_cards[5][0] < 25 && nagare_count < 3) {
                keepJudge_cards = 1;
              }
            } else {
              //提出手が温存対象か判定
              //自分の持つ強い手札が2枚以下&&使う手札の強さがK以上&&使う手札=場の最強ではない
              if (my_strongqty <= 2 && use_strength >= 11 && use_strength < max) {
                keepJudge_cards = 1;
              }
            }
            //階段か判定
            for (i = 0; i <= 4; i++) {
              for (j = 1; j <= 13; j++) {
                if (select_cards[i][j] == 1) {
                  samesuit += select_cards[i][j];
                  //ジョーカー使用の場合
                } else if (select_cards[i][j] == 2) {
                  samesuit += 1;
                }
              }
              //同じスートの手札が3枚以上=階段
              if (samesuit >= 3) {
                kaidanflag = 1;
              }
              samesuit = 0;
            }
            //強いペア（orトリプル）か判断
            for (i = 12; i <= 13; i++) {
              for (j = 0; j < 4; j++) {
                if (select_cards[j][i] == 1) {
                  samestrong += select_cards[j][i];
                } else if (select_cards[j][i] == 2) {
                  samestrong += 1;
                }
              }
              if (samestrong == 2 || samestrong == 3) {
                strongpair_flag = 1;
              }
              samestrong = 0;
            }
            if (kaidanflag || strongpair_flag) {
              keepJudge_cards = 0; //階段or強い組だったら温存しない 
            }
            //1位予測まで
            if (keepJudge_cards && nagare_count < 4 && init_valueJudge == 1) {
              onzon++;
	      clearTable(select_cards); //温存
            } else if (nagare_count >= 3 && top_seatnum > 0 && win_playernum < 1 && keepJudge_cards && value2Judge == 1) {
              // -1入ったときのためにif文分けた
              if (ss.pass[top_seatnum] != 3) {
                onzon++;
		random_judge = random() % 100;
                if (random_judge < (int)(correct * 100.0)) {
		  clearTable(select_cards); //温存
		}
	      }
	    }
	  }
	}
        /////////////////////////////////////////////////////////////
        //アルゴリズムはここまで
        /////////////////////////////////////////////////////////////
        accept_flag = sendCards(select_cards); //cardsを提出
      } else {
        //自分のターンではない時
        //必要ならここに処理を記述する

        //////////////murata(senujutu == 0)////////////////////////////////////
        if (senjutu == 0) {
          if (state.onset == 1) {
            onset_flag = 1;
          }
        }
        //////////////////////////////////////////////////////////////////////////
      }
      //そのターンに提出された結果のテーブル受け取り,場に出たカードの情報を解析する
      lookField(ba_cards_buf);
      copyTable(ba_cards, ba_cards_buf);

      ///////////////////////////////////////////////////////////////
      //カードが出されたあと 誰かがカードを出す前の処理はここに書く
      ///////////////////////////////////////////////////////////////
      last_playernum = getLastPlayerNum(ba_cards);

      if (senjutu == 3 && attack_flag == 1) {
            for (j = 0; j < 3; j++) {     
                for (k = 0; k < 4; k++) { 
                     tes[j][k][2] -= 100; // 足した分を引いて元に戻す
                }
            }
            attack_flag = 0; // フラグをおろす
          }

      if (senjutu == 0) { 
        //client_log[2][4][14][4]に提出カード格納
        if (onset_flag == 1) {
          if (state.qty == 1) {
            for (i = 0; i <= 4; i++) {
              for (j = 0; j <= 14; j++) {
                if (state.ord == j && state.suit[i] == 1) {
                  if (state.rev == 0) {
                    client_log[0][i][j][last_playernum] = 1;
                  } else if (state.rev == 1) {
                    client_log[0][i][j][last_playernum] = 2;
                  }
                }
              }
            }
          } else if (state.qty == 2) {
            for (i = 0; i <= 4; i++) {
              for (j = 0; j <= 14; j++) {
                if (state.ord == j && state.suit[i] == 1) {
                  if (state.rev == 0) {
                    client_log[1][i][j][last_playernum] = 1;
                  } else if (state.rev == 1) {
                    client_log[1][i][j][last_playernum] = 2;
                  }
                }
              }
            }
          }
          onset_flag = 0;
        }
      } else if (senjutu == 1) { 
        tes[0][1][2] = 0;
        tes[0][2][2] = 0;
        tes[0][3][2] = 0;
        tes[1][1][2] = -45;
        tes[1][2][2] = -15;
        tes[1][3][2] = -15;
        tes[2][1][2] = 0;
        tes[2][2][2] = 0;
        tes[2][3][2] = 0;
      } else if (senjutu == 2) { 
        tes[0][1][2] = 0;
        tes[0][2][2] = 0;
        tes[0][3][2] = 0;
        tes[1][1][2] = 0;
        tes[1][2][2] = 0;
        tes[1][3][2] = 0;
        tes[2][1][2] = -65;
        tes[2][2][2] = -45;
        tes[2][3][2] = -33;
      } else if (senjutu == 3) { 
        senjutu = 3;

        if (f == 0 || f == 1) { //フラグで管理する
          if (f == 0) {         //自身の手札提出ターン
            //ゲームの前後半を記録
            j = used_cards[5][1];
            if (j >= 0 && j < 13)
              t = 0; //序盤
            else if (j >= 13 && j < 32)
              t = 1; //中盤
            else if (j >= 32)
              t = 2; //終盤

            copyTable(ba_cards_bef, ba_cards); //一ターン目から差異がでるのはまずいのでここで
            if (state.rev == 0) {
              if (state.joker == 3 && state.qty == 1) { //Joker単品の場合
                tes[t][0][0] = 14;
                y = 0;
                break;
              }
              for (i = 13; i > 0; i--) {
                if (ba_cards[0][i] + ba_cards[1][i] + ba_cards[2][i] + ba_cards[3][i] > 0) { //強さ記憶
                  if (state.qty == 1) {                                                      //単体の場合
                    tes[t][1][0] = i;
                    y = 1;
                    break;
                  } else if (state.qty > 1) {  //複数枚の場合
                    if (state.sequence == 1) { //階段の場合
                      tes[t][3][0] = i;
                      y = 3;
                      break;
                    } else { //ペアの場合
                      tes[t][2][0] = i;
                      y = 2;
                      break;
                    }
                  }
                }
              }
            } else if (state.rev == 1) { //革命時
              for (i = 1; i <= 13; i++) {
                if (state.joker == 3 && state.qty == 1) { //Joker単体の場合
                  tes[t][0][0] = 14;
                  y = 0;
                  break;
                }
                if (ba_cards[0][i] + ba_cards[1][i] + ba_cards[2][i] + ba_cards[3][i] > 0) { //強さ記憶
                  if (state.qty == 1) {                                                      //単体の場合
                    tes[t][1][0] = i;
                    y = 1;
                    break;
                  } else if (state.qty > 1) {  //複数枚の場合
                    if (state.sequence == 1) { //階段の場合
                      tes[t][3][0] = i;
                      y = 3;
                      break;
                    } else { //ペアの場合
                      tes[t][2][0] = i;
                      y = 2;
                      break;
                    }
                  }
                }
              }
            }
            f = 1; //フラグを立ててこのループに入れないようにする
          }        //f==0，役提出ターン専用条件分岐終了
          pass_num++;
          f = 2;
          for (i = 0; i < 5; i++) {
            if (i == last_playernum)
              continue; //自分を除く
            if (ss.pass[i] == 0)
              f = 1; //他プレイヤがパスをしていない場合，途中で提出役が負けたことがわかる
          }
          if (cmpCards(ba_cards_bef, ba_cards) == 1 || f == 2) { //役が負けたor全員パスで場が流れた
            pass_num -= 2;
            tes[t][y][1] = pass_num; //自身が提出した役で他プレイヤがパスをした回数を格納
            /*------------------------------------------------------*/
            /*                      評価値計算                       */
            /*------------------------------------------------------*/
            if (state.rev == 0) {
              if (t == 0)
                tes[t][y][2] += 0; //ゲーム進行状況加点
              else if (t == 1)
                tes[t][y][2] -= 2;
              else if (t == 2)
                tes[t][y][2] -= 2;
              if (tes[t][y][0] == 1)
                tes[t][y][2] -= 3; //数値加点
              else if (tes[t][y][0] == 2)
                tes[t][y][2] += 1;
              else if (tes[t][y][0] >= 3 && tes[t][y][0] < 9)
                tes[t][y][2] += 8 - tes[t][y][0];
              else if (tes[t][y][0] >= 9 && tes[t][y][0] < 13)
                tes[t][y][2] += 9 - tes[t][y][0];
              else
                tes[t][y][2] += 10 - tes[t][y][0];
              if (tes[t][y][1] == 0)
                tes[t][y][2] -= 3; //パス数加点
              else if (tes[t][y][1] == 1)
                tes[t][y][2] += 1;
              else if (tes[t][y][1] == 2)
                tes[t][y][2] += 2;
              else
                tes[t][y][2] += 3;
              if (y == 0)
                tes[t][y][2] -= 0; //役加点
              else if (y == 1)
                tes[t][y][2] -= 1;
              else if (y == 2)
                tes[t][y][2] -= 2;
              else if (y == 3)
                tes[t][y][2] -= 3;
              if (state.lock == 1)
                tes[t][y][2] -= 2; //しばり加点
            } else if (state.rev == 1) {
              if (t == 0)
                tes[t][y][2] += 0; //ゲーム進行状況加点
              else if (t == 1)
                tes[t][y][2] -= 2;
              else if (t == 2)
                tes[t][y][2] -= 2;
              if (tes[t][y][0] == 13)
                tes[t][y][2] -= 3; //数値加点
              if (tes[t][y][0] == 12)
                tes[t][y][2] += 1;
              else if (tes[t][y][0] <= 11 && tes[t][y][0] > 5)
                tes[t][y][2] += tes[t][y][0] - 6;
              else if (tes[t][y][0] <= 7 && tes[t][y][0] > 1)
                tes[t][y][2] += tes[t][y][0] - 5;
              else
                tes[t][y][2] += tes[t][y][0] - 4;
              if (tes[t][y][1] == 0)
                tes[t][y][2] -= 3; //パス数加点
              else if (tes[t][y][1] == 1)
                tes[t][y][2] += 1;
              else if (tes[t][y][1] == 2)
                tes[t][y][2] += 2;
              else
                tes[t][y][2] += 3;
              if (y == 0)
                tes[t][y][2] -= 0; //役加点
              else if (y == 1)
                tes[t][y][2] -= 1;
              else if (y == 2)
                tes[t][y][2] -= 2;
              else if (y == 3)
                tes[t][y][2] -= 3;
              if (state.lock == 1)
                tes[t][y][2] -= 2; //しばり加点
            }

            //パラメータの閾値設定
            for (i = 0; i < 3; i++) { //評価値が±各閾値を超えないようにする
              for (j = 0; j < 4; j++) {
                if (i == 0) {
                  if (tes[i][j][2] > 9)
                    tes[i][j][2] = 9;
                  else if (tes[i][j][2] < -9)
                    tes[i][j][2] = -9;
                } else if (i == 1) {
                  if (j == 1) {
                    if (tes[i][j][2] > 45)
                      tes[i][j][2] = 45;
                    else if (tes[i][j][2] < -45)
                      tes[i][j][2] = -45;
                  } else {
                    if (tes[i][j][2] > 15)
                      tes[i][j][2] = 15;
                    else if (tes[i][j][2] < -15)
                      tes[i][j][2] = -15;
                  }
                } else {
                  if (j == 1) {
                    if (tes[i][j][2] > 65)
                      tes[i][j][2] = 65;
                    else if (tes[i][j][2] < -65)
                      tes[i][j][2] = -65;
                  } else if (j == 2) {
                    if (tes[i][j][2] > 45)
                      tes[i][j][2] = 45;
                    else if (tes[i][j][2] < -45)
                      tes[i][j][2] = -45;
                  } else if (j == 3) {
                    if (tes[i][j][2] > 35)
                      tes[i][j][2] = 35;
                    else if (tes[i][j][2] < -35)
                      tes[i][j][2] = -35;
                  }
                }
              }
            }
            f = 3;
            pass_num = 0;
            i = 5;
          }
        }
      } else if (senjutu == 4) { 
        senjutu = 4;
      } else { 
        senjutu = 5;
      }
    
      //提出後のカードテーブルに場のカードを記録
      ba_cut = 0;
      for (i = 0; i <= 4; i++) {
        for (j = 0; j <= 14; j++) {
          if (ba_cards[i][j] == 1) {
            used_cards[i][j] = 1;
            if (j == 6) { //8切り
              ba_cut = 1;
            } else if (i == 0 && j == 1 && jk1card == 1) { //ジョーカーへのスペード3
              ba_cut = 1;
            }
          }
          if (ba_cards[i][j] == 2) {
            used_cards[4][14] = 1;
            if (j == 6) { //8切り
              ba_cut = 1;
            }
          }
        }
      }

      used_cards[5][1] = 0; //場に出たカードの合計枚数を格納
      for (j = 1; j <= 13; j++) {
        used_cards[4][j] = used_cards[0][j] + used_cards[1][j] + used_cards[2][j] + used_cards[3][j];
        used_cards[5][1] += used_cards[4][j];
      }
      used_cards[5][1] += used_cards[4][14];

      //場がジョーカー1枚の場合のみフラグを1に保つ
      if ((state.ord == 0 || state.ord == 14) && state.qty == 1) {
        jk1card = 1;
      }
      for (i = 0; i < 5; i++) { //ターンプレイヤの席番号を格納
        if (ss.seat[i] == own_cards_buf[5][3]) {
          seat_num = i;
        }
      }

      //used_cards[5][0]=直前のターンまでに場に出たカードの合計枚数
      if (used_cards[5][0] == used_cards[5][1]) { //ターンの前後でカードが出た枚数が同じとき(パス)
        ss.pass[seat_num] = 1;
        if ((PRINT_STATE == 1 && my_playernum == 0) || PRINT_STATE == 2) {
          fprintf(stderr, " turn :P%d:Pass\n", ba_cards[5][3]);
        }
      } else { //そうでない場合提出したプレイヤの手札枚数を減らす
        ss.hand_qty[seat_num] -= (used_cards[5][1] - used_cards[5][0]);
        if ((PRINT_STATE == 1 && my_playernum == 0) || PRINT_STATE == 2) {
          fprintf(stderr, " turn :P%d:use:", ba_cards[5][3]);
          if (jk1card == 1) {
            fprintf(stderr, " jk\n");
          } else {
            cardprint(ba_cards);
          }
        }
      }
      //枚数が0になった場合(あがり)
      if (ss.hand_qty[seat_num] == 0 && ss.pass[seat_num] != 3) {
        ranks[ss.seat[seat_num]] = 0;
        for (i = 0; i < 5; i++) {
          if (ss.pass[i] == 3) {        //あがっている数カウント
            ranks[ss.seat[seat_num]]++; //順位記録
          }
        }
        ss.pass[seat_num] = 3; //あがり状態とする
        win_flag = 1;
        win_playernum++; //上がった人数記録
      } else {
        win_flag = 0;
      }
      used_cards[5][0] = used_cards[5][1]; //場に出たカードの合計枚数更新

      for (i = 0; i < 5; i++) {
        if (ss.pass[i] == 0) {
          break;
        }
      }
      if (i == 5) { //全員パス
        ba_cut = 1;

        ///////////murata(senjutu == 0)////////////////////////////////////////////////
        if (senjutu == 0) {
          if (ba_nagare_last_playernum != 10) {
            //席順戦略でパスした時の最後に出したクライアント（前のクライアント）が最終的に場を流しているか
            if (last_playernum == ba_nagare_last_playernum) {
              //予定通り流れた
              pass_check_lag[0][game_count]++;
              ba_nagare_last_playernum = 10;
            } else {
              //予定通り流れなかった
              pass_check_lag[1][game_count]++;
              ba_nagare_last_playernum = 10;
            }
          }
          ///////////////////////////////////////////////////////////////////////////////
        }
      }

      //全員パスまたは8切り、ジョーカーへのスペード3の場合パス状態とフラグリセット
      if (ba_cut == 1) {
        for (i = 0; i < 5; i++) {
          if (ss.pass[i] <= 2) {
            ss.pass[i] = 0;
          }
        }
        jk1card = 0;
        win_flag = 0;
        if (senjutu == 5) {
          nagare_count++;          //流れた回数を記録
          if (nagare_count == 2) {
            valueflag = 1;
          }
          if (nagare_count <= 3) {
            nagashi_whether[ss.seat[last_playernum]][nagare_count - 1] = 1; //場を流したプレイヤを記録（席順に)
          }

          //強いプレイヤの推測 
          if (nagare_count == 4 && judgeflag == 0) {
            judgeflag = 1;
            for (i = 0; i < 5; i++) {
              // 2回は流していた時
              if (nagashi_whether[i][0] + nagashi_whether[i][1] + nagashi_whether[i][2] == 2) {
                strong[i] += 2;
              }
              // 3連続で場を流していた時
              else if (nagashi_whether[i][0] + nagashi_whether[i][1] + nagashi_whether[i][2] == 3) {
                strong[i] += 10;
              }
              //手札が4枚減っていた時
              if (init_handqty[i] - ss.hand_qty[i] == 4) {
                strong[i] += 2;
              }
              //手札が5枚減っていた時
              else if (init_handqty[i] - ss.hand_qty[i] == 5) {
                strong[i] += 4;
              }
	      //手札が6枚減っていた時
              else if (init_handqty[i] - ss.hand_qty[i] == 6) {
                strong[i] += 6;
              }
              //手札が7枚以上減っていた時
              else if (init_handqty[i] - ss.hand_qty[i] >= 7) {
                strong[i] += 9 + 4 * (init_handqty[i] - ss.hand_qty[i] - 6);
              }
            }
            strong_max = strong[0];
            if (strong_max >= 6) {
              top_seatnum = 0; //自分
            } else {          
              top_seatnum = -1;
            }
            for (i = 1; i < 5; i++) {
              //強いプレイヤの条件に合致している場合の強さ度の最小値は6
              if (strong[i] > strong_max && strong[i] >= 6) {
                strong_max = strong[i];
                top_seatnum = i;
              }
            }
          }
        }
      }

      ///////////////////////////////////////////////////////////////
      //ここまで
      ///////////////////////////////////////////////////////////////

      //一回のゲームが終わったか否かの通知をサーバからうける。
      switch (beGameEnd()) {
      case 0: //0のときゲームを続ける
        one_gameend_flag = 0;
        whole_gameend_flag = 0;
        break;

      case 1: //1のとき 1ゲームの終了
        one_gameend_flag = 1;
        whole_gameend_flag = 0;
        if (g_logging == 1) {
          printf("game #%d was finished.\n", game_count);
        }

        //1位と予測したプレイヤの獲得階級を記録
        if (senjutu == 5) {
          if (top_seatnum != -1) 
          {
            top_ranks[ss.seat[top_seatnum]][ranks[ss.seat[top_seatnum]]]++;
          }
        }
        //使う変数の初期化
        value2 = init_value = 100;
        keepJudge_cards = nagare_count = win_playernum = 0;
        init_valueJudge = value2Judge = strong_max = valueflag = judgeflag = 0; 
        top_seatnum = -1;                                                       
        for (i = 0; i < 5; i++) {
          init_handqty[i] = strong[i] = 0;
          for (j = 0; j < 3; j++) {
            nagashi_whether[i][j] = 0;
          }
        }
        break;

      default: //その他の場合 全ゲームの終了
        one_gameend_flag = 1;
        whole_gameend_flag = 1;
        if (g_logging == 1) {
          printf("All game was finished(Total %d games.)\n", game_count);
        }
	break;
      }
    } //1ゲームが終わるまでの繰り返しここまで

    //////////////////////murata(senjutu == 0)//////////////////////////
    if (senjutu == 0) {
      for (i = 0; i <= 4; i++) {
        for (j = 0; j <= 14; j++) {
          for (k = 0; k <= 4; k++) {
            client_log[0][i][j][k] = 0;
            client_log[1][i][j][k] = 0;
          }
        }
      }
    }
    /////////////////////////////////////////////////////////////////////

    for (i = 0; i < 5; i++) {
      score[i] += (5 - ranks[i]);
    }

    //::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

    gscore[senjutu] += (5 - ranks[my_playernum]);
    ruikei[senjutu] += (5 - ranks[my_playernum]);
    shiaisuu[senjutu]++;
    shiai[senjutu]++;

    if (game_count % 100 == 0 && game_count != 3000) 
    {
      //100試合で戦術ごとの勝率と一番勝率のよい戦術を計算
      for (i = 0; i < 6; i++) {
        shouritu[i] = (double)gscore[i] / (double)shiai[i];
      }
      maxscore = shouritu[0];
      for (i = 0; i < 6; i++) {
        if (maxscore < shouritu[i]) {
          maxscore = shouritu[i];
          maxsenjutu = i;
        }
      }
    }

    //3000試合で1~3番目に高い戦術ごとの勝率を計算
    if (game_count == 3000) 
    {
      for (i = 0; i < 6; i++) {
        shouritu[i] = (double)ruikei[i] / (double)shiaisuu[i];
      }
      for (i = 0; i < 6; i++) {
        if (NO[0] < shouritu[i]) {
          NO[2] = NO[1];
          NO[1] = NO[0];
          NO[0] = shouritu[i];
          NOsenjutu[2] = NOsenjutu[1];
          NOsenjutu[1] = NOsenjutu[0];
          NOsenjutu[0] = i;
        } else if (NO[1] < shouritu[i]) {
          NO[2] = NO[1];
          NO[1] = shouritu[i];
          NOsenjutu[2] = NOsenjutu[1];
          NOsenjutu[1] = i;
        } else if (NO[2] < shouritu[i]) {
          NO[2] = shouritu[i];
          NOsenjutu[2] = i;
        }
      }
    }

    //:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

    /*------------------------------------------------------*/
    /*              ランクによる評価値の条件分岐              */
    /*------------------------------------------------------*/
    if (ranks[my_playernum] < ss.old_rank[my_playernum]) { //前ランクよりも自身のランクが高くなったら評価値の変化を記憶
      for (i = 0; i < 3; i++) {
        for (j = 0; j < 4; j++) {
          for (k = 0; k < 3; k++) {
            tes_buf[i][j][k] = tes[i][j][k];
          }
        }
      }
    } else { //ランク維持か以前より低くなったら評価値の変化をbufで上書き
      for (i = 0; i < 3; i++) {
        for (j = 0; j < 4; j++) {
          for (k = 0; k < 3; k++) {
            tes[i][j][k] = tes_buf[i][j][k];
          }
        }
      }
    }
  } //全ゲームが終わるまでの繰り返しここまで

  if ((PRINT_SCORE == 1 && my_playernum == 0) || PRINT_SCORE == 2) {
    fprintf(stderr, "     Final score "); //スコア表示
    for (i = 0; i < 5; i++) {
      fprintf(stderr, "%5d", score[i]);
      if (i == 4) {
        fprintf(stderr, "\n");
      } else {
        fprintf(stderr, " / ");
      }
    }
  }

  //ソケットを閉じて終了
  if (closeSocket() != 0) {
    printf("failed to close socket\n");
    exit(1);
  }
  exit(0);
}
