//*daifugo*/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdlib.h>

#include "daihinmin.h"
#include "connection.h"

extern const int g_logging;

extern struct state_type state;
extern struct seatsort_state ss;

//////////murata(senjutu == 0)///////////////////////
//状況表示用(1の時表示、0の時非表示)
#define status_print 0

int seat_tactics_check_on = 0;
int seat_tactics_check_off = 0;
int ba_nagare_single = 0;
int ba_nagare_double = 0;
int ba_nagare_three_cards = 0;
int ba_nagare_max_count = 0;
int ba_nagare_max_count_single = 0;
int ba_nagare_max_count_single_nomal = 0;
int ba_nagare_max_count_single_nomal_rev = 0;
int ba_nagare_max_count_single_sibari = 0;
int ba_nagare_max_count_single_sibari_rev = 0;
int ba_nagare_max_count_double = 0;
int ba_nagare_max_count_double_nomal = 0;
int ba_nagare_max_count_double_nomal_rev = 0;
int ba_nagare_max_count_double_sibari = 0;
int ba_nagare_max_count_double_sibari_rev = 0;
int flag_count_normal = 0;
int seat_tactics_check[100000][3] = {0};
int pass_check_count = 0;
int ba_nagare_last_playernum = 10;

int seat_tactics_flag = 0;

int client_log[2][4][14][5] = {0};
int client_log_st_pass_off_count = 0;

///////////////////////////////////////////////////

void getState(int cards[8][15])
{
	/*
    カードテーブルから得られる情報を読み込む
    引数は手札のカードテーブル
    情報は広域変数stateに格納される
  */
	int i;
	//状態
	if (cards[5][4] > 0)
		state.onset = 1; //場にカードがないとき 1
	else
		state.onset = 0;
	if (cards[5][6] > 0)
		state.rev = 1; //革命状態の時 1
	else
		state.rev = 0;
	if (cards[5][5] > 0)
		state.b11 = 1; //11バック時 1 未使用
	else
		state.b11 = 0;
	if (cards[5][7] > 0)
		state.lock = 1; //しばり時 1
	else
		state.lock = 0;

	if (state.onset == 1)
	{ //新たな場のとき札の情報をリセット
		state.qty = 0;
		state.ord = 0;
		state.lock = 0;
		for (i = 0; i < 5; i++)
			state.suit[i] = 0;
	}

	for (i = 0; i < 5; i++)
		state.player_qty[i] = cards[6][i]; //手持ちのカード
	for (i = 0; i < 5; i++)
		state.player_rank[i] = cards[6][5 + i]; //各プレーヤのランク
	for (i = 0; i < 5; i++)
		state.seat[i] = cards[6][10 + i]; //誰がどのシートに座っているか
										  //シートiにプレーヤ STATE.SEAT[I]が座っている

	if (cards[4][1] == 2)
		state.joker = 1; //Jokerがある時 1
	else
		state.joker = 0;
}

void getField(int cards[8][15])
{
	/*
    場に出たカードの情報を得る。
    引数は場に出たカードのテーブル
    情報は広域変数stateに格納される
  */
	int i, j, count = 0;
	int found_joker = 0;
	i = j = 0;

	//カードのある位置を探す
	while (j < 15 && cards[i][j] == 0)
	{
		state.suit[i] = 0;
		i++;
		if (i == 4)
		{
			j++;
			i = 0;
		}
	}

	//見つけたカードがジョーカーならば、found_joker=1。
	if (cards[i][j] == 2)
	{
		found_joker = 1;
	}

	//階段が否か
	if (j < 14)
	{
		if (cards[i][j + 1] > 0)
			state.sequence = 1;
		else
			state.sequence = 0;
	}
	//枚数を数える また強さを調べる
	if (state.sequence == 0)
	{
		//枚数組
		for (; i < 5; i++)
		{
			if (cards[i][j] > 0)
			{
				count++;
				state.suit[i] = 1;
			}
			else
			{
				state.suit[i] = 0;
			}
		}

		//ジョーカー単騎が場に出ているならば、state.ordを最大の強さを示すものに設定
		//ノーマルカードの場合は、その強さをそのままstate.ordへ格納
		if ((found_joker == 1) && (count == 1))
		{
			if (state.rev == 0)
			{
				state.ord = 14;
			}
			else
			{
				state.ord = 0;
			}
		}
		else
		{
			state.ord = j;
		}
	}
	else
	{
		//階段
		while (j + count < 15 && cards[i][j + count] > 0)
		{
			count++;
		}
		if ((state.rev == 0 && state.b11 == 0) || (state.rev == 1 && state.b11 == 1))
		{
			state.ord = j + count - 1;
		}
		else
		{
			state.ord = j;
		}
		state.suit[i] = 1;
	}
	//枚数を記憶
	state.qty = count;

	if (state.qty > 0)
	{ //枚数が0より大きいとき 新しい場のフラグを0にする
		state.onset = 0;
	}
}

void showState(struct state_type *state)
{
	/*引数で渡された状態stateの内容を表示する*/
	int i;
	printf("state rev   : %d\n", state->rev);
	printf("state lock  : %d\n", state->lock);
	printf("state joker : %d\n", state->joker);

	printf("state qty   : %d\n", state->qty);
	printf("state ord   : %d\n", state->ord);
	printf("state seq   : %d\n", state->sequence);
	printf("state onset : %d\n", state->onset);
	printf("state suit :");
	for (i = 0; i < 4; i++)
		printf("%d ", state->suit[i]);
	printf("\n");
	printf("state player qty :");
	for (i = 0; i < 5; i++)
		printf("%d ", state->player_qty[i]);
	printf("\n");
	printf("state player rank :");
	for (i = 0; i < 5; i++)
		printf("%d ", state->player_rank[i]);
	printf("\n");
	printf("state player_num on seat :");
	for (i = 0; i < 5; i++)
		printf("%d ", state->seat[i]);
	printf("\n");
}

//それぞれカードの和 共通 差分 逆転 をとる
void cardsOr(int cards1[8][15], int cards2[8][15])
{
	/*
    cards1にcards2にあるカードを加える
  */
	int i, j;

	for (i = 0; i < 15; i++)
		for (j = 0; j < 5; j++)
			if (cards2[j][i] > 0)
				cards1[j][i] = 1;
}

void cardsAnd(int cards1[8][15], int cards2[8][15])
{
	/*
    cards1のカードのうち、cards2にあるものだけをcards1にのこす。
  */
	int i, j;

	for (i = 0; i < 15; i++)
		for (j = 0; j < 5; j++)
			if (cards1[j][i] == 1 && cards2[j][i] == 1)
				cards1[j][i] = 1;
			else
				cards1[j][i] = 0;
}

void cardsDiff(int cards1[8][15], int cards2[8][15])
{
	/*
    cards1からcards2にあるカードを削除する
  */
	int i, j;

	for (i = 0; i < 15; i++)
		for (j = 0; j < 5; j++)
			if (cards2[j][i] == 1)
				cards1[j][i] = 0;
}
void cardsNot(int cards[8][15])
{
	/*
    カードの有無を反転させる
  */
	int i, j;

	for (i = 0; i < 15; i++)
		for (j = 0; j < 5; j++)
			if (cards[j][i] == 1)
				cards[j][i] = 0;
			else
				cards[j][i] = 1;
}

void outputTable(int table[8][15])
{
	/*
    引数で渡されたカードテーブルを出力する
  */
	int i, j;
	for (i = 0; i < 8; i++)
	{
		for (j = 0; j < 15; j++)
		{
			printf("%i ", table[i][j]);
		}
		printf("\n");
	}
	printf("\n");
}

void copyTable(int dest_table[8][15], int org_table[8][15])
{
	/*
    引数で渡されたカードテーブルorg_tableを
    カードテーブルdest_tableにコピーする
  */
	int i, j;
	for (i = 0; i < 8; i++)
	{
		for (j = 0; j < 15; j++)
		{
			dest_table[i][j] = org_table[i][j];
		}
	}
}

void copyCards(int dest_cards[8][15], int org_cards[8][15])
{
	/*
    引数で渡されたカードテーブルorg_cardsのカード情報の部分を
    カードテーブルdest_cardsにコピーする
  */
	int i, j;
	for (i = 0; i < 5; i++)
	{
		for (j = 0; j < 15; j++)
		{
			dest_cards[i][j] = org_cards[i][j];
		}
	}
}

void clearCards(int cards[8][15])
{
	/*
    引数で渡されたカードテーブルcardsのカード情報の部分を全て0にし、カードを一枚も無い状態にする。
  */
	int s, t;

	for (s = 0; s < 5; s++)
	{
		for (t = 0; t < 15; t++)
		{
			cards[s][t] = 0;
		}
	}
}

void clearTable(int cards[8][15])
{
	/*
    引数で渡されたカードテーブルcardsを全て0にする。
  */
	int s, t;

	for (s = 0; s < 8; s++)
	{
		for (t = 0; t < 15; t++)
		{
			cards[s][t] = 0;
		}
	}
}

int beEmptyCards(int cards[8][15])
{
	/*
    引数で渡されたカードテーブルcardsの含むカードの枚数が0のとき1を、
    それ以外のとき0を返す
  */
	int i, j, f = 1;

	for (i = 0; i < 5; i++)
	{
		for (j = 0; j < 15; j++)
		{
			if (cards[i][j] > 0)
				f = 0;
		}
	}
	return f;
}

int qtyOfCards(int cards[8][15])
{
	/*
    引数で渡されたカードテーブルcardsの含むカードの枚数を返す
  */
	int i, j, count = 0;

	for (i = 0; i < 5; i++)
		for (j = 0; j < 15; j++)
			if (cards[i][j] > 0)
				count++;

	return count;
}

void makeJKaidanTable(int tgt_cards[][15], int my_cards[][15])
{
	/*
    渡されたカードテーブルmy_cardsから、ジョーカーを考慮し階段で出せるかどうかを解析し、
    結果をテーブルtgt_cardsに格納する。
  */
	int i, j;
	int count, noJcount; //ジョーカーを使用した場合のカードの枚数,使用しない枚数

	clearTable(tgt_cards); //テーブルのクリア
	if (state.joker == 1)
	{ //jokerがあるとき
		for (i = 0; i < 4; i++)
		{ //各スート毎に走査し
			count = 1;
			noJcount = 0;
			for (j = 13; j >= 0; j--)
			{ //順番にみて
				if (my_cards[i][j] == 1)
				{			 //カードがあるとき
					count++; //2つのカウンタを進める
					noJcount++;
				}
				else
				{						  //カードがないとき
					count = noJcount + 1; //ジョーカーありの階段の枚数にジョーカー分を足す
					noJcount = 0;		  //ジョーカーなしの階段の枚数をリセットする
				}

				if (count > 2)
				{							 //3枚以上のとき
					tgt_cards[i][j] = count; //その枚数をテーブルに格納
				}
				else
				{
					tgt_cards[i][j] = 0; //その他は0にする
				}
			}
		}
	}

	if (g_logging == 1)
	{
		printf("make Joker kaidan \n");
		outputTable(tgt_cards);
	}
}

void makeKaidanTable(int tgt_cards[][15], int my_cards[][15])
{
	/*
    渡されたカードテーブルmy_cardsから、階段で出せるかどうかを解析し、
    結果をテーブルtgt_cardsに格納する。
  */
	int i, j;
	int count;

	clearTable(tgt_cards);
	for (i = 0; i < 4; i++)
	{ //各スート毎に走査し
		for (j = 13, count = 0; j > 0; j--)
		{ //順番にみて
			if (my_cards[i][j] == 1)
			{			 //カードがあるとき
				count++; //カウンタを進め
			}
			else
			{
				count = 0; //カードがないときリセットする
			}

			if (count > 2)
			{ //3枚以上のときその枚数をテーブルに格納
				tgt_cards[i][j] = count;
			}
			else
			{
				tgt_cards[i][j] = 0; //その他は0にする
			}
		}
	}
	if (g_logging == 1)
	{
		printf("make kaidan \n");
		outputTable(tgt_cards);
	}
}

void makeGroupTable(int tgt_cards[][15], int my_cards[][15])
{
	/*
    渡されたカードテーブルmy_cardsから、2枚以上の枚数組で出せるかどうかを解析し、
    結果をテーブルtgt_cardsに格納する。
  */
	int i, j;
	int count;

	clearTable(tgt_cards);
	for (i = 0; i < 15; i++)
	{ //それそれの強さのカードの枚数を数え
		count = my_cards[0][i] + my_cards[1][i] + my_cards[2][i] + my_cards[3][i];
		if (count > 1)
		{ //枚数が2枚以上のとき
			for (j = 0; j < 4; j++)
			{
				if (my_cards[j][i] == 1)
				{							 //カードを持っている部分に
					tgt_cards[j][i] = count; //その枚数を格納
				}
			}
		}
	}
	if (g_logging == 1)
	{
		printf("make group \n");
		outputTable(tgt_cards);
	}
}

void makeJGroupTable(int tgt_cards[][15], int my_cards[][15])
{
	/*
    渡されたカードテーブルmy_cardsから、
    ジョーカーを考慮し2枚以上の枚数組で出せるかどうかを解析し、
    結果をテーブルtgt_cardsに格納する。
  */
	int i, j;
	int count;

	clearTable(tgt_cards);
	if (state.joker != 0)
	{
		for (i = 0; i < 14; i++)
		{ //それそれの強さのカードの枚数を数え ジョーカーの分を加える
			count = my_cards[0][i] + my_cards[1][i] + my_cards[2][i] + my_cards[3][i] + 1;
			if (count > 1)
			{ //枚数が2枚以上のとき
				for (j = 0; j < 4; j++)
				{
					if (my_cards[j][i] == 1)
					{							 //カードを持っている部分に
						tgt_cards[j][i] = count; //その枚数を格納
					}
				}
			}
		}
	}
	if (g_logging == 1)
	{
		printf("make Joker group \n");
		outputTable(tgt_cards);
	}
}

void lowCards(int out_cards[8][15], int my_cards[8][15], int threshold)
{
	/*
    渡されたカードテーブルmy_cardsのカード部分を
    threshold以上の部分は0でうめ,thresholdより低い部分をのこし、
    カードテーブルout_cardsに格納する。
  */
	int i;
	copyTable(out_cards, my_cards); //my_cardsをコピーして
	for (i = threshold; i < 15; i++)
	{						 //thresholdから15まで
		out_cards[0][i] = 0; //0でうめる
		out_cards[1][i] = 0;
		out_cards[2][i] = 0;
		out_cards[3][i] = 0;
	}
}

void highCards(int out_cards[8][15], int my_cards[8][15], int threshold)
{
	/*
    渡されたカードテーブルmy_cardsのカード部分を
    threshold以下の部分は0でうめ,thresholdより高い部分をのこし
    カードテーブルout_cardsに格納する
  */
	int i;
	copyTable(out_cards, my_cards); //my_cardsをコピーして
	for (i = 0; i <= threshold; i++)
	{						 //0からthresholdまで
		out_cards[0][i] = 0; //0でうめる
		out_cards[1][i] = 0;
		out_cards[2][i] = 0;
		out_cards[3][i] = 0;
	}
}
int nCards(int n_cards[8][15], int target[8][15], int n)
{
	/*
    n枚のペアあるいは階段のみをn_cards にのこす。このときテーブルにのる数字はnのみ。
    カードが無いときは0,あるときは1をかえす。
  */
	int i, j, flag = 0;
	clearTable(n_cards); //テーブルをクリア
	for (i = 0; i < 4; i++)
		for (j = 0; j < 15; j++) //テーブル全体を走査し
			if (target[i][j] == (int)n)
			{ //nとなるものをみつけたとき
				n_cards[i][j] = n;
				flag = 1; //フラグをたて
			}
			else
			{					   //n以外の場所は
				n_cards[i][j] = 0; //0で埋める。
			}
	return flag;
}

void lockCards(int target_cards[8][15], int suit[5])
{
	/*
    大域変数state.suitの１が立っているスートのみカードテーブルtarget_cardsに残す。
  */
	int i, j;
	for (i = 0; i < 4; i++)
		for (j = 0; j < 15; j++)
			target_cards[i][j] *= suit[i]; //suit[i]==1 のときはそのまま,==0のとき0である。
}

void lowGroup(int out_cards[8][15], int my_cards[8][15], int group[8][15])
{
	/*
    渡された枚数組で出せるカードの情報をのせたgroupとカードテーブルmy_cardsから
    最も低い枚数組を探し、見つけたらカードテーブルout_cardsにそのカードを載せる。
  */
	int i, j;				//カウンタ
	int count = 0, qty = 0; //カードの枚数,総数

	clearTable(out_cards);
	for (j = 1; j < 14; j++)
	{ //ランクが低い順に探索する
		for (i = 0; i < 4; i++)
		{
			if (group[i][j] > 1)
			{						 //groupテーブルに2以上の数字を発見したら
				out_cards[i][j] = 1; //out_cardsにフラグを立てる
				count++;
				qty = group[i][j];
			}
		}
		if (count > 0)
			break; //ループ脱出用フラグが立っていたら
	}

	for (i = 0; count < qty; i++)
	{
		if (my_cards[i][j] == 0 && (state.lock == 0 || state.suit[i] == 1))
		{
			out_cards[i][j] = 2; //ジョーカー用フラグを立てる
			count++;
		}
	}
}

void highGroup(int out_cards[8][15], int my_cards[8][15], int group[8][15])
{
	/*
    渡された枚数組で出せるカードの情報をのせたgroupとカードテーブルmy_cardsから
    最も高い枚数組を探し、見つけたらカードテーブルout_cardsにそのカードを載せる。
  */
	int i, j;				//カウンタ
	int count = 0, qty = 0; //カードの枚数,総数

	clearTable(out_cards);
	for (j = 13; j > 0; j--)
	{ //ランクが低い順に探索する
		for (i = 0; i < 4; i++)
		{
			if (group[i][j] > 1)
			{						 //groupテーブルに2以上の数字を発見したら
				out_cards[i][j] = 1; //out_cardsにフラグを立てる
				count++;
				qty = group[i][j];
			}
		}
		if (count > 0)
			break; //ループ脱出用フラグが立っていたら
	}

	for (i = 0; count < qty; i++)
	{
		if (my_cards[i][j] == 0 && (state.lock == 0 || state.suit[i] == 1))
		{
			out_cards[i][j] = 2; //ジョーカー用フラグを立てる
			count++;
		}
	}
}

void lowSequence(int out_cards[8][15], int my_cards[8][15], int sequence[8][15])
{
	/*
    渡された階段で出せるカードの情報をのせたgroupとカードテーブルmy_cardsから
    最も低い階段を探し、見つけたらカードテーブルout_cardsにそのカードを載せる。
  */
	int i, j, lowvalue, lowline = 0, lowcolumn = 0;

	lowvalue = 0;

	clearTable(out_cards);
	i = 0;

	//lowsequenceの発見
	while ((i < 15) && (lowvalue == 0))
	{ //階段テーブル中に階段が見つかるまで繰り返し
		j = 0;
		while (j < 4)
		{
			if (sequence[j][i] != 0)
			{ //低い数字から調べ,階段テーブルが0以外だったら分岐
				if (sequence[j][i] > lowvalue)
				{							   //同じ数字を起点として作られる階段の中で最長か否か
					lowvalue = sequence[j][i]; //最長だったら値と場所を保存
					lowline = j;
					lowcolumn = i;
				}
			}
			j++;
		}
		if (lowvalue == 0)
		{
			i++;
		}
	}

	//out_cardsへの書出し
	if (lowvalue != 0)
	{ //階段が見つからなかったらout_cardsには書出さない
		for (i = lowcolumn; i < (lowcolumn + lowvalue); i++)
		{
			if (my_cards[lowline][i] == 1)
			{
				out_cards[lowline][i] = 1; //普通の手札として持っていたら1を立てる
			}
			else
			{
				out_cards[lowline][i] = 2; //持っていなかったらジョーカーなので2を立てる
			}
		}
	}
}

void highSequence(int out_cards[8][15], int my_cards[8][15], int sequence[8][15])
{
	/*
    渡された階段で出せるカードの情報をのせたgroupとカードテーブルmy_cardsから
    最も高い階段を探し、見つけたらカードテーブルout_cardsにそのカードを載せる。
  */
	int i, j, k, highvalue, highline = 0, highcolumn = 0, prevalue;
	highvalue = 0;

	clearTable(out_cards);
	i = 14;

	//highsequenceの発見
	while ((i > 0) && (highvalue == 0))
	{ //階段テーブル中に階段が見つかるまで繰り返し
		j = 0;
		while (j < 4)
		{
			k = -1;
			if ((sequence[j][i] != 0) && (my_cards[j][i] != 0))
			{ //高い数字から調べ,階段テーブルが0以外だったら分岐
				do
				{ //見つけた階段の最高値から,最長の階段を探す
					if (sequence[j][i - k] >= highvalue)
					{									//同じ最高値を持つ階段の中で最長か否か
						highvalue = sequence[j][i - k]; //最長だったら記録
						highline = j;
						highcolumn = i - k;
					}
					prevalue = sequence[j][i - k];
					k++;
				} while ((i - k >= 0) && (prevalue <= sequence[j][i - k]));
			}
			j++;
		}
		if (highvalue == 0)
		{
			i--;
		}
	}

	//out_cardsへの書出し
	for (i = highcolumn; i < (highcolumn + highvalue); i++)
	{
		if (my_cards[highline][i] == 1)
		{
			out_cards[highline][i] = 1; //普通の手札として持っていたら1を立てる
		}
		else
		{
			out_cards[highline][i] = 2; //持っていなかったらジョーカーなので2を立てる
		}
	}
}

//my_cards(手札)からペア,階段等の役のカードを除去したものをout_cardに格納する
void removeGroup(int out_cards[8][15], int my_cards[8][15], int group[8][15])
{
	/*
    渡された枚数組で出せるカードの情報をのせたgroupとカードテーブルmy_cardsから
    枚数組以外のカードを探し、カードテーブルout_cardsにそのカードを載せる。
  */
	int i, j;

	for (i = 0; i < 15; i++)
	{
		for (j = 0; j < 4; j++)
		{
			if ((my_cards[j][i] == 1) && (group[j][i] == 0))
			{
				out_cards[j][i] = 1; //mycardsに存在し,かつ役テーブルにない場合1
			}
			else
			{
				out_cards[j][i] = 0; //それ以外(mycardsにないか,役テーブルにある)の場合0
			}
		}
	}
}

void removeSequence(int out_cards[8][15], int my_cards[8][15], int sequence[8][15])
{
	/*
    渡された階段で出せるカードの情報をのせたgroupとカードテーブルmy_cardsから
    階段以外のカードを探し、カードテーブルout_cardsにそのカードを載せる。
  */
	int i, j, k;

	for (j = 0; j < 4; j++)
	{
		for (i = 0; i < 15; i++)
		{
			if ((my_cards[j][i] == 1) && (sequence[j][i] == 0))
			{
				out_cards[j][i] = 1; //mycardsに存在し,かつ役テーブルにない場合1
			}
			else if (sequence[j][i] > 2)
			{
				for (k = 0; k < sequence[j][i]; k++)
				{
					out_cards[j][i + k] = 0;
				}
				i += k - 1;
			}
			else
			{
				out_cards[j][i] = 0; //それ以外(mycardsにないか,役テーブルにある)の場合0
			}
		}
	}
}

void lowSolo(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{
	/*
    低い方から探して,最初に見つけたカードを一枚out_cardsにのせる。
    joker_flagが1のとき,カードが見つからなければ,jokerを一枚out_cardsにのせる。
  */
	int i, j, find_flag = 0;

	clearTable(out_cards); //テーブルをクリア
	for (j = 1; j < 14 && find_flag == 0; j++)
	{ //低い方からさがし
		for (i = 0; i < 4 && find_flag == 0; i++)
		{
			if (my_cards[i][j] == 1)
			{									  //カードを見つけたら
				find_flag = 1;					  //フラグを立て
				out_cards[i][j] = my_cards[i][j]; //out_cardsにのせ,ループを抜ける。
			}
		}
	}
	if (find_flag == 0 && joker_flag == 1)
	{						  //見つからなかったとき
		out_cards[0][14] = 2; //ジョーカーをのせる
	}
}

void highSolo(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{
	/*
    高い方から探して,最初に見つけたカードを一枚out_cardsにのせる。
    joker_flagがあるとき,カードが見つからなければ,jokerを一枚out_cardsにのせる。
  */
	int i, j, find_flag = 0;

	clearTable(out_cards); //テーブルをクリア
	for (j = 13; j > 0 && find_flag == 0; j--)
	{ //高い方からさがし
		for (i = 0; i < 4 && find_flag == 0; i++)
		{
			if (my_cards[i][j] == 1)
			{									  //カードを見つけたら
				find_flag = 1;					  //フラグを立て
				out_cards[i][j] = my_cards[i][j]; //out_cardsにのせ,ループを抜ける。
			}
		}
	}
	if (find_flag == 0 && joker_flag == 1)
	{						 //見つからなかったとき
		out_cards[0][0] = 2; //ジョーカーをのせる
	}
}

void change(int out_cards[8][15], int my_cards[8][15], int num_of_change)
{
	/*
    カード交換時のアルゴリズム
    大富豪あるいは富豪が、大貧民あるいは貧民にカードを渡す時のカードを
    カードテーブルmy_cardsと交換枚数num_of_changeに応じて、
    低いほうから選びカードテーブルout_cardsにのせる
  */
	int count = 0;
	int one_card[8][15];

	clearTable(out_cards);
	while (count < num_of_change)
	{
		lowSolo(one_card, my_cards, 0);
		cardsDiff(my_cards, one_card);
		cardsOr(out_cards, one_card);
		count++;
	}
}

void lead(int out_cards[8][15], int my_cards[8][15])
{
	/*
    新しくカードを提出するときの選択ルーチン
    カードテーブルmy_cardsから階段=>ペア=>一枚の順で枚数の多いほうから走査し,
    低いカードからみて、はじめて見つけたものを out_cardsにのせる。
  */
	int group[8][15];	  //枚数組を調べるためのテーブル
	int sequence[8][15];  //階段を調べるためのテーブル
	int temp[8][15];	  //一時使用用のテーブル
	int i, find_flag = 0; //手札が発見したか否かのフラグ

	clearTable(group);
	clearTable(sequence);
	clearTable(temp);
	if (state.joker == 1)
	{										  //ジョーカーがあるとき,ジョーカーを考慮し,
		makeJGroupTable(group, my_cards);	  //階段と枚数組があるかを調べ,
		makeJKaidanTable(sequence, my_cards); //テーブルに格納する
	}
	else
	{
		makeGroupTable(group, my_cards);	 //ジョーカーがないときの階段と枚数組の
		makeKaidanTable(sequence, my_cards); //状況をテーブルに格納する
	}

	for (i = 15; i >= 3 && find_flag == 0; i--)
	{										   //枚数の大きい方から,見つかるまで
		find_flag = nCards(temp, sequence, i); //階段があるかをしらべ,

		if (find_flag == 1)
		{											//見つかったとき
			lowSequence(out_cards, my_cards, temp); //そのなかで最も低いものをout_cards
		}											//にのせる
	}
	for (i = 5; i >= 2 && find_flag == 0; i--)
	{										//枚数の大きい方から,見つかるまで
		find_flag = nCards(temp, group, i); //枚数組があるかを調べ,
		if (find_flag == 1)
		{										 //見つかったとき
			lowGroup(out_cards, my_cards, temp); //そのなかで最も低いものをout_cards
		}										 //のせる
	}
	if (find_flag == 0)
	{											   //まだ見つからないとき
		lowSolo(out_cards, my_cards, state.joker); //最も低いカードをout_cardsにのせる。
	}
}

void leadRev(int out_cards[8][15], int my_cards[8][15])
{
	/*
    革命時用の新しくカードを提出するときの選択ルーチン
    カードテーブルmy_cardsから階段=>ペア=>一枚の順で枚数の多いほうから走査し,
    高いカードからみて、はじめて見つけたものを out_cardsにのせる。
  */
	int group[8][15];	  //枚数組を調べるためのテーブル
	int sequence[8][15];  //階段を調べるためのテーブル
	int temp[8][15];	  //一時使用用のテーブル
	int i, find_flag = 0; //手札が発見したか否かのフラグ
	//clearTable(group);
	//clearTable(sequence);
	//clearTable(temp);
	if (state.joker == 1)
	{										  //ジョーカーがあるとき,ジョーカーを考慮し,
		makeJGroupTable(group, my_cards);	  //階段と枚数組があるかを調べ,
		makeJKaidanTable(sequence, my_cards); //テーブルに格納する
	}
	else
	{
		makeGroupTable(group, my_cards);	 //ジョーカーがないときの階段と枚数組の
		makeKaidanTable(sequence, my_cards); //状況をテーブルに格納する
	}
	for (i = 15; i >= 3 && find_flag == 0; i--)
	{ //枚数の大きい方から,見つかるまで

		find_flag = nCards(temp, sequence, i); //階段があるかをしらべ,

		if (find_flag == 1)
		{											 //見つかったとき
			highSequence(out_cards, my_cards, temp); //そのなかで最も高いものをout_cards
		}											 //にのせる
	}
	for (i = 5; i >= 2 && find_flag == 0; i--)
	{										//枚数の大きい方から,見つかるまで
		find_flag = nCards(temp, group, i); //枚数組があるかを調べ,
		if (find_flag == 1)
		{										  //見つかったとき
			highGroup(out_cards, my_cards, temp); //そのなかで最も高いものをout_cards
		}										  //にのせる
	}
	if (find_flag == 0)
	{												//まだ見つからないとき
		highSolo(out_cards, my_cards, state.joker); //最も高いカードをout_cardsにのせる
	}
}

void followSolo(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{
	/*
    他のプレーヤーに続いてカードを一枚で出すときのルーチン
    joker_flagが1の時ジョーカーを使おうとする
    提出するカードはカードテーブルout_cardsに格納される
  */
	int group[8][15];	 //枚数組を調べるためのテーブル
	int sequence[8][15]; //階段を調べるためのテーブル
	int temp[8][15];	 //一時使用用のテーブル

	makeGroupTable(group, my_cards);	 //枚数組を書き出す
	makeKaidanTable(sequence, my_cards); //階段を書き出す

	removeSequence(temp, my_cards, sequence); // 階段を除去
	removeGroup(out_cards, temp, group);	  // 枚数組を除去

	highCards(temp, out_cards, state.ord); // 場のカードより弱いカードを除去

	if (state.lock == 1)
	{
		lockCards(temp, state.suit); //ロックされているとき出せないカードを除去
	}
	lowSolo(out_cards, temp, state.joker); //残ったカードから弱いカードを抜き出す
}

void followGroup(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{
	/*  
    他のプレーヤーに続いてカードを枚数組で出すときのルーチン
    joker_flagが1の時ジョーカーを使おうとする
    提出するカードはカードテーブルout_cardsに格納される
  */
	int group[8][15];
	int ngroup[8][15];
	int temp[8][15];

	highCards(temp, my_cards, state.ord); //場より強いカードを残す
	if (state.lock == 1)
	{								 //ロックされているとき
		lockCards(temp, state.suit); //出せないカードを除去
	}
	makeGroupTable(group, temp); //残ったものから枚数組を書き出す
	if (nCards(ngroup, group, state.qty) == 0 && state.joker == 1)
	{
		//場と同じ枚数の組が無いときジョーカーを使って探す
		makeJGroupTable(group, temp);
		nCards(ngroup, group, state.qty); //場と同じ枚数の組のみのこす。
	}
	lowGroup(out_cards, my_cards, ngroup); //一番弱い組を抜き出す
}

void followSequence(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{
	/*
    他のプレーヤーに続いてカードを階段で出すときのルーチン
    joker_flagが1の時ジョーカーを使おうとする
    提出するカードはカードテーブルout_cardsに格納される
  */
	int seq[8][15];
	int nseq[8][15];
	int temp[8][15];

	highCards(temp, my_cards, state.ord); //場より強いカードを残す
	if (state.lock == 1)
	{								 //ロックされているとき
		lockCards(temp, state.suit); //出せないカードを除去
	}
	makeKaidanTable(seq, temp); //階段を書き出す
	if (nCards(nseq, seq, state.qty) == 0 && state.joker == 1)
	{
		//場と同じ枚数の階段が無いときジョーカーを使って探す
		makeJKaidanTable(seq, temp);
		nCards(nseq, seq, state.qty); //場と同じ枚数の組のみのこす。
	}
	lowSequence(out_cards, my_cards, nseq); //一番弱い階段を
}

void followSoloRev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{
	/*
    革命状態のときに他のプレーヤーに続いてカードを一枚で出すときのルーチン
    joker_flagが1の時ジョーカーを使おうとする
    提出するカードはカードテーブルout_cardsに格納される
  */
	int group[8][15];
	int sequence[8][15];
	int temp[8][15];

	makeGroupTable(group, my_cards);	 //枚数組を書き出す
	makeKaidanTable(sequence, my_cards); //階段を書き出す

	removeSequence(temp, my_cards, sequence); // 階段を除去
	removeGroup(out_cards, temp, group);	  // 枚数組を除去
	lowCards(temp, out_cards, state.ord);	  // 場のカードより強いカードを除去
	if (state.lock == 1)
	{
		lockCards(temp, state.suit); //ロックされているとき出せないカードを除去
	}
	highSolo(out_cards, temp, state.joker); //残ったカードから強いカードを抜き出す
}

void followGroupRev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{
	/*
    革命状態のときに他のプレーヤーに続いてカードを枚数組で出すときのルーチン
    joker_flagが1の時ジョーカーを使おうとする
    提出するカードはカードテーブルout_cardsに格納される
  */
	int group[8][15];
	int ngroup[8][15];
	int temp[8][15];

	lowCards(temp, my_cards, state.ord); //場より弱いカードを残す
	if (state.lock == 1)
	{								 //ロックされているとき
		lockCards(temp, state.suit); //出せないカードを除去
	}
	makeGroupTable(group, temp); //枚数組を書き出す
	if (nCards(ngroup, group, state.qty) == 0 && state.joker == 1)
	{
		//場と同じ枚数の組が無いときジョーカーを使って探す
		makeJGroupTable(group, temp);
		nCards(ngroup, group, state.qty); //場と同じ枚数の組のみのこす。
	}
	highGroup(out_cards, my_cards, ngroup); //残ったものから一番強い組を抜き出す
}

void followSequenceRev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{
	/*
    革命状態のときに他のプレーヤーに続いてカードを階段で出すときのルーチン
    joker_flagが1の時ジョーカーを使おうとする
    提出するカードはカードテーブルout_cardsに格納される
  */
	int seq[8][15];
	int nseq[8][15];
	int temp[8][15];

	lowCards(temp, my_cards, state.ord); //場より弱いカードを残す
	if (state.lock == 1)
	{								 //ロックされているとき
		lockCards(temp, state.suit); //出せないカードを除去
	}
	makeKaidanTable(seq, temp); //階段を書き出す
	if (nCards(nseq, seq, state.qty) == 0 && state.joker == 1)
	{
		//場と同じ枚数の階段が無いときジョーカーを使って探す
		makeJKaidanTable(seq, temp);
		nCards(nseq, seq, state.qty); //場と同じ枚数の階段のみのこす。
	}
	highSequence(out_cards, my_cards, nseq); //残ったものから一番強い組を抜き出す
}

void follow(int out_cards[8][15], int my_cards[8][15])
{
	/*
    他のプレーヤーに続いてカードを出すときのルーチン
    場の状態stateに応じて一枚、枚数組、階段の場合に分けて
    対応すれる関数を呼び出す
    提出するカードはカードテーブルout_cardsに格納される
  */
	clearTable(out_cards);
	if (state.qty == 1)
	{
		followSolo(out_cards, my_cards, state.joker); //一枚のとき
	}
	else
	{
		if (state.sequence == 0)
		{
			followGroup(out_cards, my_cards, state.joker); //枚数組のとき
		}
		else
		{
			followSequence(out_cards, my_cards, state.joker); //階段のとき
		}
	}
}

void followRev(int out_cards[8][15], int my_cards[8][15])
{
	/*
    他のプレーヤーに続いてカードを出すときのルーチン
    場の状態stateに応じて一枚、枚数組、階段の場合に分けて
    対応すれる関数を呼び出す
    提出するカードはカードテーブルout_cardsに格納される
  */
	clearTable(out_cards);
	if (state.qty == 1)
	{
		followSoloRev(out_cards, my_cards, state.joker); //一枚のとき
	}
	else
	{
		if (state.sequence == 0)
		{
			followGroupRev(out_cards, my_cards, state.joker); //枚数組のとき
		}
		else
		{
			followSequenceRev(out_cards, my_cards, state.joker); //階段のとき
		}
	}
}

int cmpCards(int cards1[8][15], int cards2[8][15])
{
	/*
    カードテーブルcards1、cards2のカード部分を比較し、
    異なっていれば1、一致していれば0を返す
  */
	int i, j, flag = 0;

	for (i = 0; i < 5; i++)
		for (j = 0; j < 15; j++)
			if (cards1[i][j] != cards2[i][j])
				flag = 1;

	return flag;
}

int cmpState(struct state_type *state1, struct state_type *state2)
{
	/*
    状態を格納するstate1とstate2を比較し、一致すれば0を、
    異なっていればそれ以外を返す
  */
	int i, flag = 0;
	if (state1->ord != state2->ord)
		flag += 1;
	if (state1->qty != state2->qty)
		flag += 2;
	if (state1->sequence != state2->sequence)
		flag += 4;
	for (i = 0; i < 5; i++)
		if (state1->suit[i] != state2->suit[i])
			flag += 8;
	if (state1->onset != state2->onset)
		flag += 16;
	return flag;
}

int getLastPlayerNum(int ba_cards[8][15])
{
	/*
    最後パス以外のカード提出をしたプレーヤーの番号を返す。
    この関数を正常に動作させるためには、
    サーバから場に出たカードをもらう度に
    この関数を呼び出す必要がある。
  */
	static struct state_type last_state;
	static int last_player_num = -1;

	if (g_logging == 1)
	{ //ログの表示
		printf("Now state \n");
		showState(&state);
		printf(" Last state \n");
		showState(&last_state);
	}

	if (cmpState(&last_state, &state) != 0)
	{									  //場の状態に変化が起きたら
		last_player_num = ba_cards[5][3]; //最後のプレーヤと
		last_state = state;				  //最新の状態を更新する
	}

	if (g_logging == 1)
	{ //ログの表示
		printf("last player num : %d\n", last_player_num);
	}

	return last_player_num;
}

//////////////////////////////////////////////////
////////2015年に作ったプログラムからの流用とか////////
//////////////////////////////////////////////////

void copyTable2(int dest_table[8][15], int org_table[8][15])
{
	/*
    引数で渡されたカードテーブルorg_tableを
    カードテーブルdest_tableにコピーする
  */
	int i, j;
	for (i = 0; i < 8; i++)
	{
		for (j = 0; j < 15; j++)
		{

			if (dest_table[i][j] == 0 && org_table[i][j] == 1)
			{
				dest_table[i][j] = org_table[i][j];
			}

			if (dest_table[i][j] == 1 && org_table[i][j] == 1)
			{
				dest_table[i][j] = org_table[i][j];
			}

			if (dest_table[i][j] == 2 && org_table[i][j] == 1)
			{
				dest_table[i][j] = 3;
			}

			if (dest_table[i][j] == 0 && org_table[i][j] == 2)
			{
				dest_table[i][j] = org_table[i][j];
			}

			if (dest_table[i][j] == 1 && org_table[i][j] == 2)
			{
				dest_table[i][j] = 3;
			}
		}
	}
}

void copyTable3(int dest_table[8][15], int org_table[8][15])
{
	/*
    引数で渡されたカードテーブルorg_tableを
    カードテーブルdest_tableにコピーする
  */
	int i, j;
	for (i = 0; i < 5; i++)
	{ //場の判定のため、カード部分のみ。
		for (j = 0; j < 15; j++)
		{

			if (dest_table[i][j] == 0 && org_table[i][j] == 1)
			{
				dest_table[i][j] = 1;
			}
			else if (dest_table[i][j] == 1 && org_table[i][j] == 2)
			{
				dest_table[i][j] = 3;
			}
		}
	}
	//追加
	if (state.submit_joker == 0)
	{
		for (i = 0; i < 5; i++)
		{ //場の判定のため、カード部分のみ。
			for (j = 0; j < 15; j++)
			{
				if (org_table[i][j] == 2)
				{
					state.submit_joker = 1;
					state.joker_risk = 0;

					state.submitted_cards[4][1] = 2;
					state.submitted_cards_plus_hands[4][1] = 2;
				}
			}
		}
	}

	if (state.submit_s3 == 0)
	{
		if (dest_table[0][1] == 1)
		{
			state.submit_s3 = 1;
			state.s3_risk = 0;
			state.find_s3 = 1;
		}
	}
}

void copyTable4(int dest_table[8][15], int org_table[8][15])
{
	/*
    引数で渡されたカードテーブルorg_tableを
    カードテーブルdest_tableにコピーする
  */
	int i, j;
	for (i = 0; i < 5; i++)
	{ //場の判定のため、カード部分のみ。
		for (j = 0; j < 15; j++)
		{

			if (dest_table[i][j] == 0 && org_table[i][j] == 1)
			{
				dest_table[i][j] = 1;
			}
			else if (dest_table[i][j] == 1 && org_table[i][j] == 2)
			{
				dest_table[i][j] = 3;
			}
		}
	}
	//追加
	if (state.submit_joker == 0)
	{
		for (i = 0; i < 5; i++)
		{ //場の判定のため、カード部分のみ。
			for (j = 0; j < 15; j++)
			{
				if (org_table[i][j] == 2)
				{
					state.submit_joker = 1; //自分の手札をコピーしただけで、まだ提出されてはいない・・・（後になって気づいた）
					state.joker_risk = 0;

					state.submitted_cards[4][1] = 2;
					state.submitted_cards_plus_hands[4][1] = 2;
				}
			}
		}
	}
	/*
   				if(state.submit_s3==0){
    				if(dest_table[0][1]==1){
    					state.submit_s3=1;
    					state.s3_risk=0;
    				}
   				}
	*/

	if (dest_table[0][1] == 1)
	{
		state.find_s3 = 1;
		state.s3_risk = 0;
	}
}

void copyTable5(int dest_table[8][15], int org_table[8][15])
{ //my_lead14用
	/*
    引数で渡されたカードテーブルorg_tableを
    カードテーブルdest_tableにコピーする
  */
	int i, j;
	for (i = 0; i < 5; i++)
	{ //場の判定のため、カード部分のみ。
		for (j = 0; j < 15; j++)
		{

			if (dest_table[i][j] == 0 && org_table[i][j] == 1)
			{
				dest_table[i][j] = 1;
			}
			else if (dest_table[i][j] == 1 && org_table[i][j] == 2)
			{
				dest_table[i][j] = 3;
			}
		}
	}
	//追加
	/*
	if(state.submit_joker==0){
    		for(i=0;i<5;i++){ //場の判定のため、カード部分のみ。
    			for(j=0;j<15;j++){
    				if(org_table[i][j]==2){
    					state.submit_joker=1;//自分の手札をコピーしただけで、まだ提出されてはいない・・・（後になって気づいた）
    					state.joker_risk=0;
    				
    					state.submitted_cards[4][1]=2;
    					state.submitted_cards_plus_hands[4][1]=2;
    				}
    			}
    		}
    }
	*/
	/*
   				if(state.submit_s3==0){
    				if(dest_table[0][1]==1){
    					state.submit_s3=1;
    					state.s3_risk=0;
    				}
   				}
	*/
	/*
				if(dest_table[0][1]==1){
    					state.find_s3=1;
						state.s3_risk=0;
    			}
				*/
}

void outputCards(int table[8][15])
{
	/*
    引数で渡されたカードテーブルを出力する
  */
	int i, j;
	for (i = 0; i < 5; i++)
	{
		for (j = 0; j < 15; j++)
		{
			printf("%i ", table[i][j]);
		}
		printf("\n");
	}
	printf("\n");
}

int sameCards(int dest_cards[8][15], int org_cards[8][15])
{
	//同じかどうかを判定
	int i, j, same = 1;
	for (i = 0; i < 5; i++)
	{
		for (j = 0; j < 15; j++)
		{
			if (dest_cards[i][j] != org_cards[i][j])
			{
				same = 0;
			}
		}
	}

	return same;
}

//////////////////////////////////////////////////
////////ここまで。
////////ここから新規////////
//////////////////////////////////////////////////

void clearCards2(int cards[8][15])
{
	/*
    引数で渡されたカードテーブルcardsのカード情報の部分を全て8にする。
  */
	int s, t;

	for (s = 0; s < 5; s++)
	{
		for (t = 0; t < 15; t++)
		{
			cards[s][t] = 8;
		}
	}
}

int joker_check(int table[8][15])
{

	int i, j, joker = 0;
	for (i = 0; i < 5; i++)
	{
		for (j = 0; j < 15; j++)
		{
			if (table[i][j] == 2)
				joker = 2;
		}
	}
	return joker;
}

/*
int joker_check2(int table[8][15]){ 

  int i,j,joker=0;
  for(i=0;i<5;i++){
    for(j=0;j<15;j++){
    	if(table[i][j]==2){
    		joker=2;
    		state.joker_i1=i;
    		state.joker_j1=j;
    	}
    }
  }
	if(joker==0){
		state.joker_i1=4;
    	state.joker_j1=1;
	}
  return joker;
	
}
*/

int joker_check3(int table[8][15])
{

	int i, j;
	for (i = 0; i < 5; i++)
	{
		for (j = 0; j < 15; j++)
		{
			if (table[i][j] == 2)
				printf("joker[i][j]     joker[%d][%d]\n", i, j);
		}
	}
}

int joker_delete(int table[8][15])
{

	int i, j;
	for (i = 0; i < 5; i++)
	{
		for (j = 0; j < 15; j++)
		{
			if (table[i][j] == 2)
				table[i][j] = 0;
		}
	}
}

int s3_check(int table[8][15])
{

	if (table[0][1] == 1)
	{
		state.have_s3 = 1;
	}
	else
	{
		state.have_s3 = 0;
	}
}

void player_number_count(int cards[8][15])
{

	state.player_number = 0;
	state.player_number_point = 0;

	if (cards[6][0] >= 1)
		state.player_number++;
	if (cards[6][1] >= 1)
		state.player_number++;
	if (cards[6][2] >= 1)
		state.player_number++;
	if (cards[6][3] >= 1)
		state.player_number++;
	if (cards[6][4] >= 1)
		state.player_number++;

	//人数に応じて、state.player_nunmber_pointを調整
	//if(state.player_number>=3)state.player_number_point=1;
	//if(state.player_number==5)state.player_number_point=2;

	//if(state.player_number>=4)state.player_number_point=1;

	//前回の試合の自分の階級を確認。

	//諸々
	//if(state.pre_rank==2&&state.player_number>=4)state.player_number_point=1;
	//if(state.pre_rank==1&&state.player_number==5)state.player_number_point=1;

	//printf("state.pre_rank %d\n",state.pre_rank);
	//printf("state.player_number %d\n",state.player_number);
}

int player_info6(int m, int cards[8][15])
{

	int i = 0, x = 0, a = 0, count = 0;
	/*
	for(i=0;i<15;i++){
		printf("%d\n",cards[6][i]);
	}
		printf("\n");
	*/

	for (i = 10; i < 15; i++)
	{
		if (cards[6][i] == m)
		{
			//printf("maisuu   %d\n",cards[6][m]);
			//printf("zyuni    %d\n",cards[6][m+5]);
			//printf("\n");
			x = i;

			state.player_info[count][0] = m;
			state.player_info[count][1] = cards[6][m];
			state.player_info[count][2] = cards[6][m + 5];

			count++;
		}
	}

	if (x - 1 >= 10)
	{
		a = cards[6][x - 1];
		//printf("maisuu   %d\n",cards[6][a]);
		//printf("zyuni    %d\n",cards[6][a+5]);
		//printf("\n");

		state.player_info[count][0] = a;
		state.player_info[count][1] = cards[6][a];
		state.player_info[count][2] = cards[6][a + 5];

		count++;
	}

	if (x - 2 >= 10)
	{
		a = cards[6][x - 2];
		//printf("maisuu   %d\n",cards[6][a]);
		//printf("zyuni    %d\n",cards[6][a+5]);
		//printf("\n");

		state.player_info[count][0] = a;
		state.player_info[count][1] = cards[6][a];
		state.player_info[count][2] = cards[6][a + 5];

		count++;
	}

	if (x - 3 >= 10)
	{
		a = cards[6][x - 3];
		//printf("maisuu   %d\n",cards[6][a]);
		//printf("zyuni    %d\n",cards[6][a+5]);
		//printf("\n");

		state.player_info[count][0] = a;
		state.player_info[count][1] = cards[6][a];
		state.player_info[count][2] = cards[6][a + 5];

		count++;
	}

	if (x - 4 >= 10)
	{
		a = cards[6][x - 4];
		//printf("maisuu   %d\n",cards[6][a]);
		//printf("zyuni    %d\n",cards[6][a+5]);
		//printf("\n");

		state.player_info[count][0] = a;
		state.player_info[count][1] = cards[6][a];
		state.player_info[count][2] = cards[6][a + 5];

		count++;
	}

	if (x + 4 <= 14)
	{
		a = cards[6][x + 4];
		//printf("maisuu   %d\n",cards[6][a]);
		//printf("zyuni    %d\n",cards[6][a+5]);
		//printf("\n");

		state.player_info[count][0] = a;
		state.player_info[count][1] = cards[6][a];
		state.player_info[count][2] = cards[6][a + 5];

		count++;
	}

	if (x + 3 <= 14)
	{
		a = cards[6][x + 3];
		//printf("maisuu   %d\n",cards[6][a]);
		//printf("zyuni    %d\n",cards[6][a+5]);
		//printf("\n");

		state.player_info[count][0] = a;
		state.player_info[count][1] = cards[6][a];
		state.player_info[count][2] = cards[6][a + 5];

		count++;
	}

	if (x + 2 <= 14)
	{
		a = cards[6][x + 2];
		//printf("maisuu   %d\n",cards[6][a]);
		//printf("zyuni    %d\n",cards[6][a+5]);
		//printf("\n");

		state.player_info[count][0] = a;
		state.player_info[count][1] = cards[6][a];
		state.player_info[count][2] = cards[6][a + 5];

		count++;
	}

	if (x + 1 <= 14)
	{
		a = cards[6][x + 1];
		//printf("maisuu   %d\n",cards[6][a]);
		//printf("zyuni    %d\n",cards[6][a+5]);
		//printf("\n");

		state.player_info[count][0] = a;
		state.player_info[count][1] = cards[6][a];
		state.player_info[count][2] = cards[6][a + 5];

		count++;
	}
}

int player_info6_2(int m, int cards[8][15])
{

	int i = 0, x = 0, a = 0, count = 0;
	/*
	for(i=0;i<15;i++){
		printf("%d\n",cards[6][i]);
	}
		printf("\n");
	*/

	for (i = 10; i < 15; i++)
	{
		if (cards[6][i] == m)
		{
			//printf("maisuu   %d\n",cards[6][m]);
			//printf("zyuni    %d\n",cards[6][m+5]);
			//printf("\n");
			x = i;

			state.player_info2[count][0] = m;
			state.player_info2[count][1] = cards[6][m];
			state.player_info2[count][2] = cards[6][m + 5];

			count++;
		}
	}

	if (x - 1 >= 10)
	{
		a = cards[6][x - 1];
		//printf("maisuu   %d\n",cards[6][a]);
		//printf("zyuni    %d\n",cards[6][a+5]);
		//printf("\n");

		state.player_info2[count][0] = a;
		state.player_info2[count][1] = cards[6][a];
		state.player_info2[count][2] = cards[6][a + 5];

		count++;
	}

	if (x - 2 >= 10)
	{
		a = cards[6][x - 2];
		//printf("maisuu   %d\n",cards[6][a]);
		//printf("zyuni    %d\n",cards[6][a+5]);
		//printf("\n");

		state.player_info2[count][0] = a;
		state.player_info2[count][1] = cards[6][a];
		state.player_info2[count][2] = cards[6][a + 5];

		count++;
	}

	if (x - 3 >= 10)
	{
		a = cards[6][x - 3];
		//printf("maisuu   %d\n",cards[6][a]);
		//printf("zyuni    %d\n",cards[6][a+5]);
		//printf("\n");

		state.player_info2[count][0] = a;
		state.player_info2[count][1] = cards[6][a];
		state.player_info2[count][2] = cards[6][a + 5];

		count++;
	}

	if (x - 4 >= 10)
	{
		a = cards[6][x - 4];
		//printf("maisuu   %d\n",cards[6][a]);
		//printf("zyuni    %d\n",cards[6][a+5]);
		//printf("\n");

		state.player_info2[count][0] = a;
		state.player_info2[count][1] = cards[6][a];
		state.player_info2[count][2] = cards[6][a + 5];

		count++;
	}

	if (x + 4 <= 14)
	{
		a = cards[6][x + 4];
		//printf("maisuu   %d\n",cards[6][a]);
		//printf("zyuni    %d\n",cards[6][a+5]);
		//printf("\n");

		state.player_info2[count][0] = a;
		state.player_info2[count][1] = cards[6][a];
		state.player_info2[count][2] = cards[6][a + 5];

		count++;
	}

	if (x + 3 <= 14)
	{
		a = cards[6][x + 3];
		//printf("maisuu   %d\n",cards[6][a]);
		//printf("zyuni    %d\n",cards[6][a+5]);
		//printf("\n");

		state.player_info2[count][0] = a;
		state.player_info2[count][1] = cards[6][a];
		state.player_info2[count][2] = cards[6][a + 5];

		count++;
	}

	if (x + 2 <= 14)
	{
		a = cards[6][x + 2];
		//printf("maisuu   %d\n",cards[6][a]);
		//printf("zyuni    %d\n",cards[6][a+5]);
		//printf("\n");

		state.player_info2[count][0] = a;
		state.player_info2[count][1] = cards[6][a];
		state.player_info2[count][2] = cards[6][a + 5];

		count++;
	}

	if (x + 1 <= 14)
	{
		a = cards[6][x + 1];
		//printf("maisuu   %d\n",cards[6][a]);
		//printf("zyuni    %d\n",cards[6][a+5]);
		//printf("\n");

		state.player_info2[count][0] = a;
		state.player_info2[count][1] = cards[6][a];
		state.player_info2[count][2] = cards[6][a + 5];

		count++;
	}
}

int player_info6_3(int m, int cards[8][15])
{

	int i = 0, x = 0, a = 0, count = 0;
	/*
	for(i=0;i<15;i++){
		printf("%d\n",cards[6][i]);
	}
		printf("\n");
	*/

	for (i = 10; i < 15; i++)
	{
		if (cards[6][i] == m)
		{
			//printf("maisuu   %d\n",cards[6][m]);
			//printf("zyuni    %d\n",cards[6][m+5]);
			//printf("\n");
			x = i;

			state.player_info3[count][0] = m;
			state.player_info3[count][1] = cards[6][m];
			state.player_info3[count][2] = cards[6][m + 5];

			count++;
		}
	}

	if (x - 1 >= 10)
	{
		a = cards[6][x - 1];
		//printf("maisuu   %d\n",cards[6][a]);
		//printf("zyuni    %d\n",cards[6][a+5]);
		//printf("\n");

		state.player_info3[count][0] = a;
		state.player_info3[count][1] = cards[6][a];
		state.player_info3[count][2] = cards[6][a + 5];

		count++;
	}

	if (x - 2 >= 10)
	{
		a = cards[6][x - 2];
		//printf("maisuu   %d\n",cards[6][a]);
		//printf("zyuni    %d\n",cards[6][a+5]);
		//printf("\n");

		state.player_info3[count][0] = a;
		state.player_info3[count][1] = cards[6][a];
		state.player_info3[count][2] = cards[6][a + 5];

		count++;
	}

	if (x - 3 >= 10)
	{
		a = cards[6][x - 3];
		//printf("maisuu   %d\n",cards[6][a]);
		//printf("zyuni    %d\n",cards[6][a+5]);
		//printf("\n");

		state.player_info3[count][0] = a;
		state.player_info3[count][1] = cards[6][a];
		state.player_info3[count][2] = cards[6][a + 5];

		count++;
	}

	if (x - 4 >= 10)
	{
		a = cards[6][x - 4];
		//printf("maisuu   %d\n",cards[6][a]);
		//printf("zyuni    %d\n",cards[6][a+5]);
		//printf("\n");

		state.player_info3[count][0] = a;
		state.player_info3[count][1] = cards[6][a];
		state.player_info3[count][2] = cards[6][a + 5];

		count++;
	}

	if (x + 4 <= 14)
	{
		a = cards[6][x + 4];
		//printf("maisuu   %d\n",cards[6][a]);
		//printf("zyuni    %d\n",cards[6][a+5]);
		//printf("\n");

		state.player_info3[count][0] = a;
		state.player_info3[count][1] = cards[6][a];
		state.player_info3[count][2] = cards[6][a + 5];

		count++;
	}

	if (x + 3 <= 14)
	{
		a = cards[6][x + 3];
		//printf("maisuu   %d\n",cards[6][a]);
		//printf("zyuni    %d\n",cards[6][a+5]);
		//printf("\n");

		state.player_info3[count][0] = a;
		state.player_info3[count][1] = cards[6][a];
		state.player_info3[count][2] = cards[6][a + 5];

		count++;
	}

	if (x + 2 <= 14)
	{
		a = cards[6][x + 2];
		//printf("maisuu   %d\n",cards[6][a]);
		//printf("zyuni    %d\n",cards[6][a+5]);
		//printf("\n");

		state.player_info3[count][0] = a;
		state.player_info3[count][1] = cards[6][a];
		state.player_info3[count][2] = cards[6][a + 5];

		count++;
	}

	if (x + 1 <= 14)
	{

		a = cards[6][x + 1];
		//printf("maisuu   %d\n",cards[6][a]);
		//printf("zyuni    %d\n",cards[6][a+5]);
		//printf("\n");

		state.player_info3[count][0] = a;
		state.player_info3[count][1] = cards[6][a];
		state.player_info3[count][2] = cards[6][a + 5];

		count++;
	}
}

void outputkumi_info(int kaisuu)
{
	/*
   該当するkumi_infoの中身を表示する。
  */
	if (state.kumi_info[kaisuu][1] > 0)
	{
		printf("kaisuu  %d\n", kaisuu);
		printf("state.kumi_info0   %d\n", state.kumi_info[kaisuu][0]);
		printf("state.kumi_info1   %d\n", state.kumi_info[kaisuu][1]);
		//printf("%d\n",state.kumi_info[kaisuu][2]);
		printf("state.kumi_info3   %d\n", state.kumi_info[kaisuu][3]);
		//printf("%d\n",state.kumi_info[kaisuu][4]);
		//printf("%d\n",state.kumi_info[kaisuu][5]);
		//printf("state.kumi_info6   %d\n",state.kumi_info[kaisuu][6]);
		//printf("state.kumi_info8  %d\n",state.kumi_info[kaisuu][8]);
		printf("state.kumi_info9  %d\n", state.kumi_info[kaisuu][9]);
		printf("state.kumi_info10  %d\n", state.kumi_info[kaisuu][10]);
		//printf("state.kumi_info15  %d\n",state.kumi_info[kaisuu][15]);
	}
}

void outputkumi_info2(int table[16][60])
{
	/*
    引数で渡されたカードテーブルを出力する
  */
	int i, j;
	for (i = 0; i < 16; i++)
	{
		for (j = 0; j < 60; j++)
		{
			printf("%i\t ", table[i][j]);
		}
		printf("\n");
	}
	printf("\n");
}

void clearkumi_info(int kaisuu)
{
	/*
   該当するkumi_infoの中身を消去する。
  */
	int i = 0;
	for (i = 0; i < 52; i++)
		state.kumi_info[kaisuu][i] = 0;
}

void clearkumi_info2()
{
	/*
   該当するkumi_infoの中身を消去する。
  */
	int i = 0, j = 0;

	for (i = 0; i < 16; i++)
	{
		for (j = 0; j < 60; j++)
		{
			state.kumi_info[i][j] = 0;
		}
	}
}

void copyplayer_info()
{

	int i = 0, j = 0;

	for (i = 0; i < 5; i++)
	{
		for (j = 0; j < 3; j++)
		{
			state.player_info2[i][j] = state.player_info[i][j];
		}
	}
}

void outputplayer_info(int n, int s)
{
	int j = 0;

	if (s == 1)
	{
		printf("playernum   %d\n", state.player_info[n][0]);
		printf("maisuu      %d\n", state.player_info[n][1]);
		printf("kaikyuu     %d\n", state.player_info[n][2]);
	}

	if (s == 2)
	{
		printf("playernum   %d\n", state.player_info2[n][0]);
		printf("maisuu      %d\n", state.player_info2[n][1]);
		printf("kaikyuu     %d\n", state.player_info2[n][2]);
	}
}

void clearplayer_info(int s)
{

	int i = 0, j = 0;

	if (s == 1)
	{
		for (i = 0; i < 5; i++)
		{
			for (j = 0; j < 3; j++)
			{
				state.player_info[i][j] = 0;
			}
		}
	}

	if (s == 2)
	{
		for (i = 0; i < 5; i++)
		{
			for (j = 0; j < 3; j++)
			{
				state.player_info2[i][j] = 0;
			}
		}
	}
}

int nagare_check()
{

	if (state.player_info2[0][1] > 0 || state.player_info[0][1] > 0)
	{

		if (state.player_info2[1][1] > 0 && state.player_info[1][1] == 0)
		{
			return 1;
			//printf("1\n");
		}

		if (state.player_info2[1][1] == 0 && state.player_info[1][1] == 0)
		{
			if (state.player_info2[2][1] > 0 && state.player_info[2][1] == 0)
			{
				return 2;
				//printf("2\n");
			}

			if (state.player_info2[2][1] == 0 && state.player_info[2][1] == 0)
			{
				if (state.player_info2[3][1] > 0 && state.player_info[3][1] == 0)
				{
					return 3;
					//printf("3\n");
				}

				if (state.player_info2[3][1] == 0 && state.player_info[3][1] == 0)
				{
					if (state.player_info2[4][1] > 0 && state.player_info[4][1] == 0)
					{
						return 4;
						//printf("4\n");
					}
				}
			}
		}
	}

	return 0;
}

int mae_check()
{

	if (state.player_info2[0][1] > 0 || state.player_info[0][1] > 0)
	{

		if (state.player_info2[1][1] > state.player_info[1][1])
		{
			state.mae_player_hands = state.player_info[1][1];
			return 1;
			//printf("1\n");
		}
		if (state.player_info2[1][1] == 0 && state.player_info[1][1] == 0)
		{
			if (state.player_info2[2][1] > state.player_info[2][1])
			{
				state.mae_player_hands = state.player_info[2][1];
				return 2;
				//printf("2\n");
			}

			if (state.player_info2[2][1] == 0 && state.player_info[2][1] == 0)
			{
				if (state.player_info2[3][1] > state.player_info[3][1])
				{
					state.mae_player_hands = state.player_info[3][1];
					return 3;
					//printf("3\n");
				}

				if (state.player_info2[3][1] == 0 && state.player_info[3][1] == 0)
				{
					if (state.player_info2[4][1] > state.player_info[4][1])
					{
						state.mae_player_hands = state.player_info[4][1];
						return 4;
						//printf("4\n");
					}
				}
			}
		}
	}

	return 0;
}

int pass_checks()
{

	int count = 0, flag = 0;
	int uta = 0, saigo = 0;

	if (state.onset == 1)
	{
		state.playable_player = state.player_number;
		flag = 1;
	}

	if (state.onset_flag == 1)
	{
		if (state.player_info3[0][1] > 0 || state.player_info[0][1] > 0)
		{

			if (state.player_info3[1][1] >= state.player_info[1][1] + state.qty)
				uta = 1;
			if (state.player_info3[2][1] >= state.player_info[2][1] + state.qty)
				uta = 2;
			if (state.player_info3[3][1] >= state.player_info[3][1] + state.qty)
				uta = 3;
			if (state.player_info3[4][1] >= state.player_info[4][1] + state.qty)
				uta = 4;
			//printf("uta     %d\n",uta);
			/*
			printf("state.player_info3[1][1]     %d\n",state.player_info3[1][1]);
			printf("state.player_info[1][1]     %d\n",state.player_info[1][1]);
			printf("state.player_info3[2][1]     %d\n",state.player_info3[2][1]);
			printf("state.player_info[2][1]     %d\n",state.player_info[2][1]);
			printf("state.player_info3[3][1]     %d\n",state.player_info3[3][1]);
			printf("state.player_info[3][1]     %d\n",state.player_info[3][1]);
			printf("state.player_info3[4][1]     %d\n",state.player_info3[4][1]);
			printf("state.player_info[4][1]     %d\n",state.player_info[4][1]);
			*/
			if (uta == 4)
			{
				if (state.player_info3[1][1] == state.player_info[1][1] && state.player_info[1][1] != 0)
					count++;
				if (state.player_info3[2][1] == state.player_info[2][1] && state.player_info[2][1] != 0)
					count++;
				if (state.player_info3[3][1] == state.player_info[3][1] && state.player_info[3][1] != 0)
					count++;
			}

			if (uta == 3)
			{
				if (state.player_info3[1][1] == state.player_info[1][1] && state.player_info[1][1] != 0)
					count++;
				if (state.player_info3[2][1] == state.player_info[2][1] && state.player_info[2][1] != 0)
					count++;
			}

			if (uta == 2)
			{
				if (state.player_info3[1][1] == state.player_info[1][1] && state.player_info[1][1] != 0)
					count++;
			}

			if (state.player_info3[4][1] != state.player_info[4][1])
			{
				saigo = 4;
			}
			if (state.player_info3[3][1] != state.player_info[3][1])
			{
				saigo = 3;
			}
			if (state.player_info3[2][1] != state.player_info[2][1])
			{
				saigo = 2;
			}
			if (state.player_info3[1][1] != state.player_info[1][1])
			{
				saigo = 1;
			}

			//if(saigo>0)printf("saigo   %d\n",saigo);

			state.playable_player = state.player_number - count;
			state.onset_flag = 0;
			flag = 1;
		}
	}

	if (flag == 0)
	{
		if (state.player_info3[0][1] > 0 || state.player_info[0][1] > 0)
		{

			if (state.player_info3[4][1] == state.player_info[4][1] && state.player_info[4][1] != 0)
			{
				count++;
			}
			if (state.player_info3[4][1] != state.player_info[4][1])
			{
				saigo = 4;
			}

			if (state.player_info3[3][1] == state.player_info[3][1] && state.player_info[3][1] != 0)
			{
				count++;
			}
			if (state.player_info3[3][1] != state.player_info[3][1])
			{
				saigo = 3;
			}

			if (state.player_info3[2][1] == state.player_info[2][1] && state.player_info[2][1] != 0)
			{
				count++;
			}
			if (state.player_info3[2][1] != state.player_info[2][1])
			{
				saigo = 2;
			}

			if (state.player_info3[1][1] == state.player_info[1][1] && state.player_info[1][1] != 0)
			{
				count++;
			}
			if (state.player_info3[1][1] != state.player_info[1][1])
			{
				saigo = 1;
			}

			//if(saigo>0)printf("saigo   %d\n",saigo);
			state.playable_player = state.player_number - count;
		}
	}

	//return state.playable_player;
}

int max_kumi_info(int n)
{

	int max = 0, i = 0, kuminumber = 12, set = 1;

	for (i = 0; i < 13; i++)
	{
		if (state.kumi_info[i][n] > max && state.kumi_info[i][set] > 0)
		{
			max = state.kumi_info[i][n];
			kuminumber = i;
		}
	}

	return kuminumber;
}

int min_kumi_info(int n)
{

	int min = 1000, i = 0, kuminumber = 12, set = 1;

	for (i = 0; i < 13; i++)
	{
		if (state.kumi_info[i][n] < min && state.kumi_info[i][set] > 0)
		{
			min = state.kumi_info[i][n];
			kuminumber = i;
		}
	}

	return kuminumber;
}

int max_kumi_info_kakou(int n)
{

	int max = 0, i = 0, kuminumber = 12, set = 1;

	int a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
	int x = 5, y = 4, z = 0, k = 4, l = 2;

	if (state.kumi_info[13][9] == 1)
		a = k;
	if (state.kumi_info[13][10] == 1)
		b = k;
	if (state.kumi_info[13][11] == 1)
		c = k;
	if (state.kumi_info[13][12] == 1)
		d = k;
	if (state.kumi_info[13][13] == 1)
		e = k;
	if (state.kumi_info[13][14] == 1)
		f = k;
	/*
	if(state.kumi_info[13][10]==2)b=l;
	if(state.kumi_info[13][11]==2)c=l;
	if(state.kumi_info[13][12]==2)d=l;
	if(state.kumi_info[13][13]==2)e=l;
	if(state.kumi_info[13][14]==2)f=l;
	*/

	for (i = 0; i < 13; i++)
	{

		/////////////
		if (state.kumi_info[i][1] == 1)
			state.kumi_info[i][16] = x * state.kumi_info[i][n] + y * state.kumi_info[13][9] - a; //シングルの組数
		if (state.kumi_info[i][1] == 2)
			state.kumi_info[i][16] = x * state.kumi_info[i][n] + y * state.kumi_info[13][10] - b; //ペア（2枚組）の組数

		if (state.kumi_info[i][1] == 3 && state.kumi_info[i][0] == 2)
			state.kumi_info[i][16] = x * state.kumi_info[i][n] + y * state.kumi_info[13][11] + z - c; //ペア（3枚組）の組数
		if (state.kumi_info[i][1] >= 4 && state.kumi_info[i][0] == 2)
			state.kumi_info[i][16] = x * state.kumi_info[i][n] + y * state.kumi_info[13][13] + z - e; //ペア（4枚組以上）の組数

		if (state.kumi_info[i][1] == 3 && state.kumi_info[i][0] == 3)
			state.kumi_info[i][16] = x * state.kumi_info[i][n] + y * state.kumi_info[13][12] + z - d; //階段（3枚組）の組数
		if (state.kumi_info[i][1] >= 4 && state.kumi_info[i][0] == 3)
			state.kumi_info[i][16] = x * state.kumi_info[i][n] + y * state.kumi_info[13][14] + z - f; //階段（4枚組以上）の組数
		///////////////

		if (state.kumi_info[i][16] > max && state.kumi_info[i][set] > 0)
		{
			max = state.kumi_info[i][16];
			kuminumber = i;
		}
	}

	return kuminumber;
}

int max_kumi_info_kakou2(int n)
{

	int max = 0, i = 0, kuminumber = 12, set = 1;

	int a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
	int x = 5, y = 4, z = 16, k = 4, l = 2;

	if (state.kumi_info[13][9] == 1)
		a = k;
	if (state.kumi_info[13][10] == 1)
		b = k;
	if (state.kumi_info[13][11] == 1)
		c = k;
	if (state.kumi_info[13][12] == 1)
		d = k;
	if (state.kumi_info[13][13] == 1)
		e = k;
	if (state.kumi_info[13][14] == 1)
		f = k;
	/*
	if(state.kumi_info[13][10]==2)b=l;
	if(state.kumi_info[13][11]==2)c=l;
	if(state.kumi_info[13][12]==2)d=l;
	if(state.kumi_info[13][13]==2)e=l;
	if(state.kumi_info[13][14]==2)f=l;
	*/

	for (i = 0; i < 13; i++)
	{

		/////////////
		if (state.kumi_info[i][1] == 1)
			state.kumi_info[i][16] = x * state.kumi_info[i][n] + y * state.kumi_info[13][9] - a; //シングルの組数
		if (state.kumi_info[i][1] == 2)
			state.kumi_info[i][16] = x * state.kumi_info[i][n] + y * state.kumi_info[13][10] - b; //ペア（2枚組）の組数

		if (state.kumi_info[i][1] == 3 && state.kumi_info[i][0] == 2)
			state.kumi_info[i][16] = x * state.kumi_info[i][n] + y * state.kumi_info[13][11] + z - c; //ペア（3枚組）の組数
		if (state.kumi_info[i][1] >= 4 && state.kumi_info[i][0] == 2)
			state.kumi_info[i][16] = x * state.kumi_info[i][n] + y * state.kumi_info[13][13] + z - e; //ペア（4枚組以上）の組数

		if (state.kumi_info[i][1] == 3 && state.kumi_info[i][0] == 3)
			state.kumi_info[i][16] = x * state.kumi_info[i][n] + y * state.kumi_info[13][12] + z - d; //階段（3枚組）の組数
		if (state.kumi_info[i][1] >= 4 && state.kumi_info[i][0] == 3)
			state.kumi_info[i][16] = x * state.kumi_info[i][n] + y * state.kumi_info[13][14] + z - f; //階段（4枚組以上）の組数
		///////////////

		if (state.kumi_info[i][16] > max && state.kumi_info[i][set] > 0)
		{
			max = state.kumi_info[i][16];
			kuminumber = i;
		}
	}

	return kuminumber;
}

int min_kumi_info_kakou(int n)
{

	int min = 1000, i = 0, kuminumber = 12, set = 1;
	int a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
	int x = 5, y = 4, z = 0, k = 4, l = 2;

	if (state.kumi_info[13][9] == 1)
		a = k;
	if (state.kumi_info[13][10] == 1)
		b = k;
	if (state.kumi_info[13][11] == 1)
		c = k;
	if (state.kumi_info[13][12] == 1)
		d = k;
	if (state.kumi_info[13][13] == 1)
		e = k;
	if (state.kumi_info[13][14] == 1)
		f = k;
	/*
	if(state.kumi_info[13][10]==2)b=l;
	if(state.kumi_info[13][11]==2)c=l;
	if(state.kumi_info[13][12]==2)d=l;
	if(state.kumi_info[13][13]==2)e=l;
	if(state.kumi_info[13][14]==2)f=l;
	*/

	for (i = 0; i < 13; i++)
	{

		//加工後の数値を[][15]に収納するようにした。

		////////////
		if (state.kumi_info[i][1] == 1)
			state.kumi_info[i][15] = x * state.kumi_info[i][n] - y * state.kumi_info[13][9] + a; //シングルの組数
		if (state.kumi_info[i][1] == 2)
			state.kumi_info[i][15] = x * state.kumi_info[i][n] - y * state.kumi_info[13][10] + b; //ペア（2枚組）の組数

		if (state.kumi_info[i][1] == 3 && state.kumi_info[i][0] == 2)
			state.kumi_info[i][15] = x * state.kumi_info[i][n] - y * state.kumi_info[13][11] - z + c; //ペア（3枚組）の組数
		if (state.kumi_info[i][1] >= 4 && state.kumi_info[i][0] == 2)
			state.kumi_info[i][15] = x * state.kumi_info[i][n] - y * state.kumi_info[13][13] - z + e; //ペア（4枚組以上）の組数

		if (state.kumi_info[i][1] == 3 && state.kumi_info[i][0] == 3)
			state.kumi_info[i][15] = x * state.kumi_info[i][n] - y * state.kumi_info[13][12] - z + d; //階段（3枚組）の組数
		if (state.kumi_info[i][1] >= 4 && state.kumi_info[i][0] == 3)
			state.kumi_info[i][15] = x * state.kumi_info[i][n] - y * state.kumi_info[13][14] - z + f; //階段（4枚組以上）の組数
		///////////////

		if (state.kumi_info[i][15] < min && state.kumi_info[i][set] > 0)
		{
			min = state.kumi_info[i][15];
			kuminumber = i;
		}
	}

	return kuminumber;
}

int min_kumi_info_kakou2(int n)
{

	int min = 1000, i = 0, kuminumber = 12, set = 1;
	int a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
	int x = 5, y = 4, z = 16, k = 4, l = 2;

	if (state.kumi_info[13][9] == 1)
		a = k;
	if (state.kumi_info[13][10] == 1)
		b = k;
	if (state.kumi_info[13][11] == 1)
		c = k;
	if (state.kumi_info[13][12] == 1)
		d = k;
	if (state.kumi_info[13][13] == 1)
		e = k;
	if (state.kumi_info[13][14] == 1)
		f = k;
	/*
	if(state.kumi_info[13][10]==2)b=l;
	if(state.kumi_info[13][11]==2)c=l;
	if(state.kumi_info[13][12]==2)d=l;
	if(state.kumi_info[13][13]==2)e=l;
	if(state.kumi_info[13][14]==2)f=l;
	*/

	for (i = 0; i < 13; i++)
	{

		//加工後の数値を[][15]に収納するようにした。

		////////////
		if (state.kumi_info[i][1] == 1)
			state.kumi_info[i][15] = x * state.kumi_info[i][n] - y * state.kumi_info[13][9] + a; //シングルの組数
		if (state.kumi_info[i][1] == 2)
			state.kumi_info[i][15] = x * state.kumi_info[i][n] - y * state.kumi_info[13][10] + b; //ペア（2枚組）の組数

		if (state.kumi_info[i][1] == 3 && state.kumi_info[i][0] == 2)
			state.kumi_info[i][15] = x * state.kumi_info[i][n] - y * state.kumi_info[13][11] - z + c; //ペア（3枚組）の組数
		if (state.kumi_info[i][1] >= 4 && state.kumi_info[i][0] == 2)
			state.kumi_info[i][15] = x * state.kumi_info[i][n] - y * state.kumi_info[13][13] - z + e; //ペア（4枚組以上）の組数

		if (state.kumi_info[i][1] == 3 && state.kumi_info[i][0] == 3)
			state.kumi_info[i][15] = x * state.kumi_info[i][n] - y * state.kumi_info[13][12] - z + d; //階段（3枚組）の組数
		if (state.kumi_info[i][1] >= 4 && state.kumi_info[i][0] == 3)
			state.kumi_info[i][15] = x * state.kumi_info[i][n] - y * state.kumi_info[13][14] - z + f; //階段（4枚組以上）の組数
		///////////////

		if (state.kumi_info[i][15] < min && state.kumi_info[i][set] > 0)
		{
			min = state.kumi_info[i][15];
			kuminumber = i;
		}
	}

	return kuminumber;
}

int qtyOfCards2(int cards[8][15])
{
	/*
    引数で渡されたカードテーブルcardsの含むカードの枚数を返す「2以上の数字となっていたらカウント」//ペア用
  */
	int i, j, count = 0;

	for (i = 0; i < 4; i++)
		for (j = 0; j < 15; j++)
			if (cards[i][j] > 1)
				count++;

	return count;
}

int qtyOfCards3(int cards[8][15])
{
	/*
    引数で渡されたカードテーブルcardsの含むカードの枚数を返す「3以上の数字となっていたらカウント」//階段用
  */
	int i, j, count = 0;

	for (i = 0; i < 4; i++)
		for (j = 0; j < 15; j++)
			if (cards[i][j] > 2)
				count++;

	return count;
}

int qtyOfCards4(int cards[8][15])
{
	/*
    引数で渡されたカードテーブルcardsの含むカードの枚数を返す//「1」のみを数える
  */
	int i, j, count = 0;

	for (i = 0; i < 5; i++)
		for (j = 0; j < 15; j++)
			if (cards[i][j] == 1)
				count++;

	return count;
}

int qtyOfCards5(int cards[8][15], int rank)
{
	/*
    引数で渡されたカードテーブルcardsの含むカードの枚数を返す//そのランクの枚数のみ数える。
  */
	int i, count = 0;

	for (i = 0; i < 4; i++)
		if (cards[i][rank] >= 1)
			count++;

	return count;
}

int qtyOfCards6(int cards[8][15], int mark, int rank)
{
	/*
    引数で渡されたところにカードがあるかの確認。
  */

	if (cards[mark][rank] >= 1)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

int qtyOfCards7(int cards[8][15], int rank)
{

	int i, j, count = 0, min = 0;
	j = rank;
	for (i = 0; i < 4; i++)
	{
		for (j = rank - 1; j > 0; j--)
		{
			if (cards[i][j] >= 1)
			{
				count++;
			}
		}
	}

	return count;
}

int qtyOfCards8(int cards[8][15], int rank)
{

	int i, j, count = 0, min = 0;
	j = rank;
	for (i = 0; i < 4; i++)
	{
		for (j = rank + 1; j < 14; j++)
		{
			if (cards[i][j] >= 1)
			{
				count++;
			}
		}
	}

	return count;
}

int qtyOfCards9(int cards[8][15])
{

	int j = 0, a = 0, b = 0, c = 0, d = 0, sum = 0;

	for (j = 1; j < 14; j++)
	{

		a = 0;
		b = 0;
		c = 0;
		d = 0;
		sum = 0;

		if (cards[0][j] != 0)
			a = 1;
		if (cards[1][j] != 0)
			b = 1;
		if (cards[2][j] != 0)
			c = 1;
		if (cards[3][j] != 0)
			d = 1;

		sum = a + b + c + d;

		if (sum == 4)
			return 1;

		if (state.player_number <= 5)
		{
			if (cards[4][1] == 2)
			{
				if (sum >= 3)
					return 1;
				//printf("test\n");
			}
		}
	}

	return 0;
}

int qtyOfCards10(int cards[8][15])
{

	int j = 0, a = 0, b = 0, c = 0, d = 0, sum = 0;
	int e = 0, f = 0, g = 0, h = 0, k = 0, l = 0, m = 0, n = 0, o = 0;
	int sum2 = 0, sum3 = 0, sum4 = 0, sum5 = 0, sum6 = 0, sum7 = 0, sum8 = 0, sum9 = 0;

	for (j = 0; j < 4; j++)
	{

		a = 0;
		b = 0;
		c = 0;
		d = 0;
		e = 0;
		f = 0;
		g = 0;
		h = 0;
		k = 0;
		l = 0;
		m = 0;
		n = 0;
		o = 0;

		sum = 0;
		sum2 = 0;
		sum3 = 0;
		sum4 = 0;
		sum5 = 0;
		sum6 = 0;
		sum7 = 0;
		sum8 = 0;
		sum9 = 0;

		if (cards[j][1] != 0)
			a = 1;
		if (cards[j][2] != 0)
			b = 1;
		if (cards[j][3] != 0)
			c = 1;
		if (cards[j][4] != 0)
			d = 1;
		if (cards[j][5] != 0)
			e = 1;
		if (cards[j][6] != 0)
			f = 1;
		if (cards[j][7] != 0)
			g = 1;
		if (cards[j][8] != 0)
			h = 1;
		if (cards[j][9] != 0)
			k = 1;
		if (cards[j][10] != 0)
			l = 1;
		if (cards[j][11] != 0)
			m = 1;
		if (cards[j][12] != 0)
			n = 1;
		if (cards[j][13] != 0)
			o = 1;

		sum = a + b + c + d + e;
		sum2 = b + c + d + e + f;
		sum3 = c + d + e + f + g;
		sum4 = d + e + f + g + h;
		sum5 = e + f + g + h + k;
		sum6 = f + g + h + k + l;
		sum7 = g + h + k + l + m;
		sum8 = h + k + l + m + n;
		sum9 = k + l + m + n + o;

		if (sum == 5)
			return 1;
		if (sum2 == 5)
			return 1;
		if (sum3 == 5)
			return 1;
		if (sum4 == 5)
			return 1;
		if (sum5 == 5)
			return 1;
		if (sum6 == 5)
			return 1;
		if (sum7 == 5)
			return 1;
		if (sum8 == 5)
			return 1;
		if (sum9 == 5)
			return 1;

		if (state.player_number <= 5)
		{
			if (cards[4][1] == 2)
			{
				if (sum >= 4)
					return 1;
				if (sum2 >= 4)
					return 1;
				if (sum3 >= 4)
					return 1;
				if (sum4 >= 4)
					return 1;
				if (sum5 >= 4)
					return 1;
				if (sum6 >= 4)
					return 1;
				if (sum7 >= 4)
					return 1;
				if (sum8 >= 4)
					return 1;
				if (sum9 >= 4)
					return 1;
				//printf("test2\n");
			}
		}
	}

	return 0;
}

int qtyOfCards11(int cards[8][15], int mark)
{

	int j = 0, box = 0;

	if (state.rev == 0)
	{

		for (j = 1; j < 14; j++)
		{
			if (state.submitted_cards[mark][j] == 0)
				box = j;
		}
	}
	else
	{

		for (j = 13; j > 0; j--)
		{
			if (state.submitted_cards[mark][j] == 0)
				box = j;
		}
	}

	//printf("%d\n",box);
	if (cards[mark][box] == 1)
		return box;

	return 0;
}

int qtyOfCards12(int cards[8][15], int rank, int mark)
{

	int i, j, count = 0, min = 0;
	j = rank;

	for (j = rank - 1; j > 0; j--)
	{
		if (cards[mark][j] >= 1)
		{
			count++;
		}
	}

	return count;
}

int qtyOfCards13(int cards[8][15], int rank, int mark)
{

	int i, j, count = 0, min = 0;
	j = rank;

	for (j = rank + 1; j < 14; j++)
	{
		if (cards[mark][j] >= 1)
		{
			count++;
		}
	}

	return count;
}

int qtyOfCards14(int cards[8][15], int rank)
{

	int i, j, count_a = 0, count_b = 0, count_c = 0, count_d = 0, suit_m = 0, r = 99;

	for (i = 0; i < 4; i++)
	{
		for (j = 1; j < rank; j++)
		{
			if (cards[i][j] == 0 && j != 6)
			{
				if (i == 0)
					count_a++;
				if (i == 1)
					count_b++;
				if (i == 2)
					count_c++;
				if (i == 3)
					count_d++;
			}
		}
	}

	if (count_a < suit_m)
	{
		suit_m = count_a;
		r = 0;
	}
	if (count_b < suit_m)
	{
		suit_m = count_b;
		r = 1;
	}
	if (count_c < suit_m)
	{
		suit_m = count_c;
		r = 2;
	}
	if (count_d < suit_m)
	{
		suit_m = count_d;
		r = 3;
	}

	return r;
}

int qtyOfCards15(int cards[8][15], int rank)
{

	int i, j, count_a = 0, count_b = 0, count_c = 0, count_d = 0, suit_m = 99, r = 99;

	for (i = 0; i < 4; i++)
	{
		for (j = rank + 1; j < 14; j++)
		{
			if (cards[i][j] == 0 && j != 6)
			{
				if (i == 0)
					count_a++;
				if (i == 1)
					count_b++;
				if (i == 2)
					count_c++;
				if (i == 3)
					count_d++;
			}
		}
	}

	if (count_a < suit_m)
	{
		suit_m = count_a;
		r = 0;
	}
	if (count_b < suit_m)
	{
		suit_m = count_b;
		r = 1;
	}
	if (count_c < suit_m)
	{
		suit_m = count_c;
		r = 2;
	}
	if (count_d < suit_m)
	{
		suit_m = count_d;
		r = 3;
	}

	return r;
}

int get_suit(int cards[8][15])
{

	int a = 0, i = 0, j = 0;

	for (i = 0; i < 4; i++)
	{
		for (j = 1; j < 14; j++)
		{
			if (cards[i][j] != 0)
			{
				a = i;
				//b=j;
			}
		}
	}
	return a;
}

int get_suitsum(int cards[8][15])
{

	int a = 0, i = 0, j = 0, suitsum = 0;

	for (i = 0; i < 4; i++)
	{
		for (j = 1; j < 14; j++)
		{
			if (cards[i][j] != 0)
			{

				if (i == 0)
					suitsum = suitsum + 1; //マークを1,2,4,8の組み合わせで表すことにした。
				if (i == 1)
					suitsum = suitsum + 2;
				if (i == 2)
					suitsum = suitsum + 4;
				if (i == 3)
					suitsum = suitsum + 8;
			}
		}
	}
	return suitsum;
}

int get_rank_max(int cards[8][15])
{

	int a = 0, i = 0, j = 0, rank = -1;

	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 15; j++)
		{
			if (cards[i][j] != 0 && j > rank)
				rank = j;
		}
	}
	return rank;
}

int get_rank_min(int cards[8][15])
{

	int a = 0, i = 0, j = 0, rank = 100;

	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 15; j++)
		{
			if (cards[i][j] != 0 && j < rank)
				rank = j;
		}
	}
	return rank;
}

void cardsOr2(int cards1[8][15], int cards2[8][15])
{
	/*
    cards1にcards2にあるカードを加える
  */
	int i, j;

	for (i = 0; i < 15; i++)
		for (j = 0; j < 5; j++)
			if (cards2[j][i] > 0)
				cards1[j][i] = cards2[j][i];
}

void cardsDiff2(int cards1[8][15], int cards2[8][15])
{
	/*
    cards1からcards2にあるカードを削除する
	//jokerも消せる
  */
	int i, j;

	for (i = 0; i < 15; i++)
		for (j = 0; j < 5; j++)
			if (cards2[j][i] != 0)
				cards1[j][i] = 0;
}

void cardsDiff3(int cards1[8][15], int cards2[8][15])
{
	/*
    cards1からcards2にあるカードを削除する
	//jokerも消せる
  */
	int i, j;

	for (i = 0; i < 15; i++)
		for (j = 0; j < 5; j++)
			if (cards2[j][i] != 0)
				cards1[j][i] = 0;

	if (joker_check(cards2) == 2)
		cards1[4][1] = 0;
}

void cardsDiff4(int cards1[8][15], int cards2[8][15])
{
	/*
    cards1からcards2にあるカードを削除する
	//jokerも消せる
  */
	int i, j;

	for (i = 0; i < 15; i++)
		for (j = 0; j < 5; j++)
			if (cards2[j][i] == 1)
				cards1[j][i] = 0;

	if (joker_check(cards2) == 2)
		cards1[4][1] = 0;
}

void cardsNot2(int cards1[8][15], int cards2[8][15])
{
	/*
    カードの有無を反転させる
  */
	int i, j;

	for (i = 1; i < 14; i++)
		for (j = 0; j < 4; j++)
			if (cards2[j][i] >= 1)
			{
				cards1[j][i] = 0;
			}
			else
			{
				cards1[j][i] = 1;
			}

	if (cards2[4][1] == 0)
	{
		cards1[4][1] = 2;
	}
	else
	{
		cards1[4][1] = 0;
	}
}

void lowSolo2(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{
	/*
    低い方から探して,最初に見つけたカードを一枚out_cardsにのせる。
    joker_flagが1のとき,カードが見つからなければ,jokerを一枚out_cardsにのせる。
  */
	int i, j, find_flag = 0;

	clearTable(out_cards);
	//テーブルをクリア
	for (j = 1; j < 14 && find_flag == 0; j++)
	{ //低い方からさがし
		for (i = 0; i < 4 && find_flag == 0; i++)
		{
			if (my_cards[i][j] == 1)
			{									  //カードを見つけたら
				find_flag = 1;					  //フラグを立て
				out_cards[i][j] = my_cards[i][j]; //out_cardsにのせ,ループを抜ける。
			}
		}
	}

	if (find_flag == 0 && joker_flag == 1)
	{ //見つからなかったとき
		if (state.find_s3 == 1)
		{						  //スペードの3の所在が確認されているなら。
			out_cards[0][14] = 2; //ジョーカーをのせる
		}
	}
}

void lowSolo2_1(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{
	/*
    低い方から探して,最初に見つけたカードを一枚out_cardsにのせる。
    joker_flagが1のとき,カードが見つからなければ,jokerを一枚out_cardsにのせる。
  */
	int i, j, find_flag = 0;

	clearTable(out_cards);
	/*
	//テーブルをクリア
   for(j=1;j<14&&find_flag==0;j++){        //低い方からさがし
    for(i=0;i<4&&find_flag==0;i++){
      if(my_cards[i][j]==1){              //カードを見つけたら               
	find_flag=1;                      //フラグを立て
	out_cards[i][j]=my_cards[i][j];   //out_cardsにのせ,ループを抜ける。
      }
    }
  }
	*/

	for (j = 1; j < 6 && find_flag == 0; j++)
	{ //低い方からさがし
		for (i = 0; i < 4 && find_flag == 0; i++)
		{
			if (my_cards[i][j] == 1)
			{									  //カードを見つけたら
				find_flag = 1;					  //フラグを立て
				out_cards[i][j] = my_cards[i][j]; //out_cardsにのせ,ループを抜ける。
			}
		}
	}

	if (find_flag == 0)
	{
		for (j = 7; j == 7 && find_flag == 0; j++)
		{ //低い方からさがし
			for (i = 0; i < 4 && find_flag == 0; i++)
			{
				if (my_cards[i][j] == 1)
				{									  //カードを見つけたら
					find_flag = 1;					  //フラグを立て
					out_cards[i][j] = my_cards[i][j]; //out_cardsにのせ,ループを抜ける。
				}
			}
		}
	}

	if (find_flag == 0)
	{
		for (j = 6; j == 6 && find_flag == 0; j++)
		{ //低い方からさがし
			for (i = 0; i < 4 && find_flag == 0; i++)
			{
				if (my_cards[i][j] == 1)
				{									  //カードを見つけたら
					find_flag = 1;					  //フラグを立て
					out_cards[i][j] = my_cards[i][j]; //out_cardsにのせ,ループを抜ける。
				}
			}
		}
	}

	if (find_flag == 0)
	{
		for (j = 8; j < 14 && find_flag == 0; j++)
		{ //低い方からさがし
			for (i = 0; i < 4 && find_flag == 0; i++)
			{
				if (my_cards[i][j] == 1)
				{									  //カードを見つけたら
					find_flag = 1;					  //フラグを立て
					out_cards[i][j] = my_cards[i][j]; //out_cardsにのせ,ループを抜ける。
				}
			}
		}
	}

	if (find_flag == 0 && joker_flag == 1)
	{ //見つからなかったとき
		if (state.find_s3 == 1)
		{						  //スペードの3の所在が確認されているなら。
			out_cards[0][14] = 2; //ジョーカーをのせる
		}
	}
}

void lowSolo2_2(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{
	/*
    低い方から探して,最初に見つけたカードを一枚out_cardsにのせる。
    joker_flagが1のとき,カードが見つからなければ,jokerを一枚out_cardsにのせる。
  */
	int i, j, find_flag = 0;

	clearTable(out_cards);
	/*
	//テーブルをクリア
   for(j=1;j<14&&find_flag==0;j++){        //低い方からさがし
    for(i=0;i<4&&find_flag==0;i++){
      if(my_cards[i][j]==1){              //カードを見つけたら               
	find_flag=1;                      //フラグを立て
	out_cards[i][j]=my_cards[i][j];   //out_cardsにのせ,ループを抜ける。
      }
    }
  }
	*/

	for (j = 1; j < 6 && find_flag == 0; j++)
	{ //低い方からさがし
		for (i = 0; i < 4 && find_flag == 0; i++)
		{
			if (my_cards[i][j] == 1)
			{									  //カードを見つけたら
				find_flag = 1;					  //フラグを立て
				out_cards[i][j] = my_cards[i][j]; //out_cardsにのせ,ループを抜ける。
			}
		}
	}

	if (find_flag == 0)
	{
		for (j = 7; j < 14 && find_flag == 0; j++)
		{ //低い方からさがし
			for (i = 0; i < 4 && find_flag == 0; i++)
			{
				if (my_cards[i][j] == 1)
				{									  //カードを見つけたら
					find_flag = 1;					  //フラグを立て
					out_cards[i][j] = my_cards[i][j]; //out_cardsにのせ,ループを抜ける。
				}
			}
		}
	}

	if (find_flag == 0)
	{
		for (j = 6; j == 6 && find_flag == 0; j++)
		{ //低い方からさがし
			for (i = 0; i < 4 && find_flag == 0; i++)
			{
				if (my_cards[i][j] == 1)
				{									  //カードを見つけたら
					find_flag = 1;					  //フラグを立て
					out_cards[i][j] = my_cards[i][j]; //out_cardsにのせ,ループを抜ける。
				}
			}
		}
	}

	if (find_flag == 0 && joker_flag == 1)
	{ //見つからなかったとき
		if (state.find_s3 == 1)
		{						  //スペードの3の所在が確認されているなら。
			out_cards[0][14] = 2; //ジョーカーをのせる
		}
	}
}

void highSolo2(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{
	/*
    高い方から探して,最初に見つけたカードを一枚out_cardsにのせる。
    joker_flagがあるとき,カードが見つからなければ,jokerを一枚out_cardsにのせる。
  */
	int i, j, find_flag = 0;

	clearTable(out_cards); //テーブルをクリア
	for (j = 13; j > 0 && find_flag == 0; j--)
	{ //高い方からさがし
		for (i = 0; i < 4 && find_flag == 0; i++)
		{
			if (my_cards[i][j] == 1)
			{									  //カードを見つけたら
				find_flag = 1;					  //フラグを立て
				out_cards[i][j] = my_cards[i][j]; //out_cardsにのせ,ループを抜ける。
			}
		}
	}
	if (find_flag == 0 && joker_flag == 1)
	{ //見つからなかったとき
		if (state.find_s3 == 1)
		{						 //スペードの3の提出が確認され
			out_cards[0][0] = 2; //ジョーカーをのせる
		}
	}
}

void highSolo2_1(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{
	/*
    高い方から探して,最初に見つけたカードを一枚out_cardsにのせる。
    joker_flagがあるとき,カードが見つからなければ,jokerを一枚out_cardsにのせる。
  */
	int i, j, find_flag = 0;

	clearTable(out_cards); //テーブルをクリア
						   /*
  for(j=13;j>0&&find_flag==0;j--){       //高い方からさがし
    for(i=0;i<4&&find_flag==0;i++){
      if(my_cards[i][j]==1){              //カードを見つけたら
	find_flag=1;                      //フラグを立て
	out_cards[i][j]=my_cards[i][j];   //out_cardsにのせ,ループを抜ける。
      }
    }
  }
	*/
	for (j = 13; j > 6 && find_flag == 0; j--)
	{ //高い方からさがし
		for (i = 0; i < 4 && find_flag == 0; i++)
		{
			if (my_cards[i][j] == 1)
			{									  //カードを見つけたら
				find_flag = 1;					  //フラグを立て
				out_cards[i][j] = my_cards[i][j]; //out_cardsにのせ,ループを抜ける。
			}
		}
	}

	if (find_flag == 0)
	{
		for (j = 5; j == 5 && find_flag == 0; j--)
		{ //高い方からさがし
			for (i = 0; i < 4 && find_flag == 0; i++)
			{
				if (my_cards[i][j] == 1)
				{									  //カードを見つけたら
					find_flag = 1;					  //フラグを立て
					out_cards[i][j] = my_cards[i][j]; //out_cardsにのせ,ループを抜ける。
				}
			}
		}
	}

	if (find_flag == 0)
	{
		for (j = 6; j == 6 && find_flag == 0; j--)
		{ //高い方からさがし
			for (i = 0; i < 4 && find_flag == 0; i++)
			{
				if (my_cards[i][j] == 1)
				{									  //カードを見つけたら
					find_flag = 1;					  //フラグを立て
					out_cards[i][j] = my_cards[i][j]; //out_cardsにのせ,ループを抜ける。
				}
			}
		}
	}

	if (find_flag == 0)
	{
		for (j = 4; j > 0 && find_flag == 0; j--)
		{ //高い方からさがし
			for (i = 0; i < 4 && find_flag == 0; i++)
			{
				if (my_cards[i][j] == 1)
				{									  //カードを見つけたら
					find_flag = 1;					  //フラグを立て
					out_cards[i][j] = my_cards[i][j]; //out_cardsにのせ,ループを抜ける。
				}
			}
		}
	}

	if (find_flag == 0 && joker_flag == 1)
	{ //見つからなかったとき
		if (state.find_s3 == 1)
		{						 //スペードの3の提出が確認され
			out_cards[0][0] = 2; //ジョーカーをのせる
		}
	}
}

void lowSolo3(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{
	/*
    低い方から探して,最初に見つけたカードを一枚out_cardsにのせる。
    joker_flagが1のとき,カードが見つからなければ,jokerを一枚out_cardsにのせる。
  */
	int i, j, find_flag = 0;

	clearTable(out_cards);
	//テーブルをクリア
	for (j = 1; j < 14 && find_flag == 0; j++)
	{ //低い方からさがし
		for (i = 0; i < 4 && find_flag == 0; i++)
		{
			if (my_cards[i][j] == 1)
			{									  //カードを見つけたら
				find_flag = 1;					  //フラグを立て
				out_cards[i][j] = my_cards[i][j]; //out_cardsにのせ,ループを抜ける。
			}
		}
	}

	if (find_flag == 0 && joker_flag == 1)
	{ //見つからなかったとき
		if (state.find_s3 == 1)
		{						 //スペードの3の所在が確認されているなら。
			out_cards[0][0] = 2; //ジョーカーをのせる
		}
	}
}

void highSolo3(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{
	/*
    高い方から探して,最初に見つけたカードを一枚out_cardsにのせる。
    joker_flagがあるとき,カードが見つからなければ,jokerを一枚out_cardsにのせる。
  */
	int i, j, find_flag = 0;

	clearTable(out_cards); //テーブルをクリア
	for (j = 13; j > 0 && find_flag == 0; j--)
	{ //高い方からさがし
		for (i = 0; i < 4 && find_flag == 0; i++)
		{
			if (my_cards[i][j] == 1)
			{									  //カードを見つけたら
				find_flag = 1;					  //フラグを立て
				out_cards[i][j] = my_cards[i][j]; //out_cardsにのせ,ループを抜ける。
			}
		}
	}
	if (find_flag == 0 && joker_flag == 1)
	{ //見つからなかったとき
		if (state.find_s3 == 1)
		{						  //スペードの3の提出が確認され
			out_cards[0][14] = 2; //ジョーカーをのせる
		}
	}
}

void lowCards2(int out_cards[8][15], int my_cards[8][15], int threshold, int mark)
{
	/*
    渡されたカードテーブルmy_cardsのカード部分を
    threshold以上の部分は0でうめ,thresholdより低い部分をのこし、
    カードテーブルout_cardsに格納する。
  */
	int i;
	copyTable(out_cards, my_cards); //my_cardsをコピーして
	for (i = threshold; i < 15; i++)
	{							//thresholdから15まで
		out_cards[mark][i] = 0; //0でうめる
	}
}

void highCards2(int out_cards[8][15], int my_cards[8][15], int threshold, int mark)
{
	/*
    渡されたカードテーブルmy_cardsのカード部分を
    threshold以下の部分は0でうめ,thresholdより高い部分をのこし
    カードテーブルout_cardsに格納する
  */
	int i;
	copyTable(out_cards, my_cards); //my_cardsをコピーして
	for (i = 0; i <= threshold; i++)
	{							//0からthresholdまで
		out_cards[mark][i] = 0; //0でうめる
	}
}

void makeJGroupTable2(int tgt_cards[][15], int my_cards[][15])
{
	/*
    渡されたカードテーブルmy_cardsから、
    ジョーカーを考慮し2枚以上の枚数組で出せるかどうかを解析し、
    結果をテーブルtgt_cardsに格納する。
  */
	int i, j;
	int count;

	clearTable(tgt_cards);
	if (state.joker != 0)
	{
		for (i = 1; i < 14; i++)
		{ //それそれの強さのカードの枚数を数え ジョーカーの分を加える
			count = my_cards[0][i] + my_cards[1][i] + my_cards[2][i] + my_cards[3][i] + 1;
			if (count > 1)
			{ //枚数が2枚以上のとき
				for (j = 0; j < 4; j++)
				{
					if (my_cards[j][i] == 1 || state.joker != 0)
					{
						tgt_cards[j][i] = count; //その枚数を格納
						if (state.lock == 1 && state.suit[j] == 0)
						{
							tgt_cards[j][i] = 0;
						}
					}
				}
			}
		}
	}
	else
	{

		for (i = 1; i < 14; i++)
		{ //それそれの強さのカードの枚数を数え
			count = my_cards[0][i] + my_cards[1][i] + my_cards[2][i] + my_cards[3][i];
			if (count > 1)
			{ //枚数が2枚以上のとき
				for (j = 0; j < 4; j++)
				{
					if (my_cards[j][i] == 1)
					{							 //カードを持っている部分に
						tgt_cards[j][i] = count; //その枚数を格納
					}
				}
			}
		}
	}
}

void Group2(int out_cards[8][15], int my_cards[8][15], int group[8][15], int rank, int qty, int pattern)
{

	int i = 0, j = 0;
	int count = 0;
	int flag = 0;

	clearTable(out_cards);

	if (qty == 2)
	{

		if (group[0][rank] == qty && group[1][rank] == qty && pattern == 0)
		{

			if (my_cards[0][rank] == 1)
				out_cards[0][rank] = 1;
			if (my_cards[1][rank] == 1)
				out_cards[1][rank] = 1;

			if (qtyOfCards(out_cards) != qty && state.joker == 1)
			{

				if (out_cards[0][rank] == 0)
				{
					out_cards[0][rank] = 2;
					flag = 1;
				}
				if (out_cards[1][rank] == 0 && flag == 0)
				{
					out_cards[1][rank] = 2;
					flag = 1;
				}
			}
		}

		if (group[0][rank] == qty && group[2][rank] == qty && pattern == 1)
		{

			if (my_cards[0][rank] == 1)
				out_cards[0][rank] = 1;
			if (my_cards[2][rank] == 1)
				out_cards[2][rank] = 1;

			if (qtyOfCards(out_cards) != qty && state.joker == 1)
			{

				if (out_cards[0][rank] == 0)
				{
					out_cards[0][rank] = 2;
					flag = 1;
				}
				if (out_cards[2][rank] == 0 && flag == 0)
				{
					out_cards[2][rank] = 2;
					flag = 1;
				}
			}
		}

		if (group[0][rank] == qty && group[3][rank] == qty && pattern == 2)
		{

			if (my_cards[0][rank] == 1)
				out_cards[0][rank] = 1;
			if (my_cards[3][rank] == 1)
				out_cards[3][rank] = 1;

			if (qtyOfCards(out_cards) != qty && state.joker == 1)
			{

				if (out_cards[0][rank] == 0)
				{
					out_cards[0][rank] = 2;
					flag = 1;
				}
				if (out_cards[3][rank] == 0 && flag == 0)
				{
					out_cards[3][rank] = 2;
					flag = 1;
				}
			}
		}

		if (group[1][rank] == qty && group[2][rank] == qty && pattern == 3)
		{

			if (my_cards[1][rank] == 1)
				out_cards[1][rank] = 1;
			if (my_cards[2][rank] == 1)
				out_cards[2][rank] = 1;

			if (qtyOfCards(out_cards) != qty && state.joker == 1)
			{

				if (out_cards[1][rank] == 0)
				{
					out_cards[1][rank] = 2;
					flag = 1;
				}
				if (out_cards[2][rank] == 0 && flag == 0)
				{
					out_cards[2][rank] = 2;
					flag = 1;
				}
			}
		}

		if (group[1][rank] == qty && group[3][rank] == qty && pattern == 4)
		{

			if (my_cards[1][rank] == 1)
				out_cards[1][rank] = 1;
			if (my_cards[3][rank] == 1)
				out_cards[3][rank] = 1;

			if (qtyOfCards(out_cards) != qty && state.joker == 1)
			{

				if (out_cards[1][rank] == 0)
				{
					out_cards[1][rank] = 2;
					flag = 1;
				}
				if (out_cards[3][rank] == 0 && flag == 0)
				{
					out_cards[3][rank] = 2;
					flag = 1;
				}
			}
		}

		if (group[2][rank] == qty && group[3][rank] == qty && pattern == 5)
		{

			if (my_cards[2][rank] == 1)
				out_cards[2][rank] = 1;
			if (my_cards[3][rank] == 1)
				out_cards[3][rank] = 1;

			if (qtyOfCards(out_cards) != qty && state.joker == 1)
			{

				if (out_cards[2][rank] == 0)
				{
					out_cards[2][rank] = 2;
					flag = 1;
				}
				if (out_cards[3][rank] == 0 && flag == 0)
				{
					out_cards[3][rank] = 2;
					flag = 1;
				}
			}
		}
	}

	if (qty == 3)
	{

		if (group[0][rank] == qty && group[1][rank] == qty && group[2][rank] == qty && pattern == 0)
		{

			if (my_cards[0][rank] == 1)
				out_cards[0][rank] = 1;
			if (my_cards[1][rank] == 1)
				out_cards[1][rank] = 1;
			if (my_cards[2][rank] == 1)
				out_cards[2][rank] = 1;

			if (qtyOfCards(out_cards) != qty && state.joker == 1)
			{

				if (out_cards[0][rank] == 0)
				{
					out_cards[0][rank] = 2;
					flag = 1;
				}
				if (out_cards[1][rank] == 0 && flag == 0)
				{
					out_cards[1][rank] = 2;
					flag = 1;
				}
				if (out_cards[2][rank] == 0 && flag == 0)
				{
					out_cards[2][rank] = 2;
					flag = 1;
				}
			}
		}

		if (group[3][rank] == qty && group[1][rank] == qty && group[2][rank] == qty && pattern == 1)
		{

			if (my_cards[3][rank] == 1)
				out_cards[3][rank] = 1;
			if (my_cards[1][rank] == 1)
				out_cards[1][rank] = 1;
			if (my_cards[2][rank] == 1)
				out_cards[2][rank] = 1;

			if (qtyOfCards(out_cards) != qty && state.joker == 1)
			{

				if (out_cards[3][rank] == 0)
				{
					out_cards[3][rank] = 2;
					flag = 1;
				}
				if (out_cards[1][rank] == 0 && flag == 0)
				{
					out_cards[1][rank] = 2;
					flag = 1;
				}
				if (out_cards[2][rank] == 0 && flag == 0)
				{
					out_cards[2][rank] = 2;
					flag = 1;
				}
			}
		}

		if (group[3][rank] == qty && group[0][rank] == qty && group[2][rank] == qty && pattern == 2)
		{

			if (my_cards[3][rank] == 1)
				out_cards[3][rank] = 1;
			if (my_cards[0][rank] == 1)
				out_cards[0][rank] = 1;
			if (my_cards[2][rank] == 1)
				out_cards[2][rank] = 1;

			if (qtyOfCards(out_cards) != qty && state.joker == 1)
			{

				if (out_cards[3][rank] == 0)
				{
					out_cards[3][rank] = 2;
					flag = 1;
				}
				if (out_cards[0][rank] == 0 && flag == 0)
				{
					out_cards[0][rank] = 2;
					flag = 1;
				}
				if (out_cards[2][rank] == 0 && flag == 0)
				{
					out_cards[2][rank] = 2;
					flag = 1;
				}
			}
		}

		if (group[3][rank] == qty && group[0][rank] == qty && group[1][rank] == qty && pattern == 3)
		{

			if (my_cards[3][rank] == 1)
				out_cards[3][rank] = 1;
			if (my_cards[0][rank] == 1)
				out_cards[0][rank] = 1;
			if (my_cards[1][rank] == 1)
				out_cards[1][rank] = 1;

			if (qtyOfCards(out_cards) != qty && state.joker == 1)
			{

				if (out_cards[3][rank] == 0)
				{
					out_cards[3][rank] = 2;
					flag = 1;
				}
				if (out_cards[0][rank] == 0 && flag == 0)
				{
					out_cards[0][rank] = 2;
					flag = 1;
				}
				if (out_cards[1][rank] == 0 && flag == 0)
				{
					out_cards[1][rank] = 2;
					flag = 1;
				}
			}
		}
	}

	if (qty == 4)
	{

		if (group[0][rank] == qty && group[1][rank] == qty && group[2][rank] == qty && group[3][rank] == qty && pattern == 0)
		{

			if (my_cards[0][rank] == 1)
				out_cards[0][rank] = 1;
			if (my_cards[1][rank] == 1)
				out_cards[1][rank] = 1;
			if (my_cards[2][rank] == 1)
				out_cards[2][rank] = 1;
			if (my_cards[3][rank] == 1)
				out_cards[3][rank] = 1;

			if (qtyOfCards(out_cards) != qty && state.joker == 1)
			{

				for (i = 0; i < 4; i++)
				{
					if (out_cards[i][rank] == 0)
					{
						out_cards[i][rank] = 2;
						break;
					}
				}
			}
		}
	}
}

void Sequence2(int out_cards[8][15], int my_cards[8][15], int mark, int qty, int pattern)
{

	int i = 0, j = 0;
	int count = 0;
	int flag = 0;
	int qty2 = 0;

	qty2 = qty;

	clearTable(out_cards);

	if (my_cards[4][1] == 0)
	{
		if (qty2 == 3)
		{
			if (pattern < 12)
			{
				if (my_cards[mark][pattern] + my_cards[mark][pattern + 1] + my_cards[mark][pattern + 2] == qty2)
				{
					out_cards[mark][pattern] = 1;
					out_cards[mark][pattern + 1] = 1;
					out_cards[mark][pattern + 2] = 1;
				}
			}
		}

		if (qty2 == 4)
		{
			if (pattern < 11)
			{
				if (my_cards[mark][pattern] + my_cards[mark][pattern + 1] + my_cards[mark][pattern + 2] + my_cards[mark][pattern + 3] == qty2)
				{
					out_cards[mark][pattern] = 1;
					out_cards[mark][pattern + 1] = 1;
					out_cards[mark][pattern + 2] = 1;
					out_cards[mark][pattern + 3] = 1;
				}
			}
		}

		if (qty2 == 5)
		{
			if (pattern < 10)
			{
				if (my_cards[mark][pattern] + my_cards[mark][pattern + 1] + my_cards[mark][pattern + 2] + my_cards[mark][pattern + 3] + my_cards[mark][pattern + 4] == qty2)
				{
					out_cards[mark][pattern] = 1;
					out_cards[mark][pattern + 1] = 1;
					out_cards[mark][pattern + 2] = 1;
					out_cards[mark][pattern + 3] = 1;
					out_cards[mark][pattern + 4] = 1;
				}
			}
		}

		if (qty2 == 6)
		{
			if (pattern < 9)
			{
				if (my_cards[mark][pattern] + my_cards[mark][pattern + 1] + my_cards[mark][pattern + 2] + my_cards[mark][pattern + 3] + my_cards[mark][pattern + 4] + my_cards[mark][pattern + 5] == qty2)
				{
					out_cards[mark][pattern] = 1;
					out_cards[mark][pattern + 1] = 1;
					out_cards[mark][pattern + 2] = 1;
					out_cards[mark][pattern + 3] = 1;
					out_cards[mark][pattern + 4] = 1;
					out_cards[mark][pattern + 5] = 1;
				}
			}
		}

		if (qty2 == 7)
		{
			if (pattern < 8)
			{
				if (my_cards[mark][pattern] + my_cards[mark][pattern + 1] + my_cards[mark][pattern + 2] + my_cards[mark][pattern + 3] + my_cards[mark][pattern + 4] + my_cards[mark][pattern + 5] + my_cards[mark][pattern + 6] == qty2)
				{
					out_cards[mark][pattern] = 1;
					out_cards[mark][pattern + 1] = 1;
					out_cards[mark][pattern + 2] = 1;
					out_cards[mark][pattern + 3] = 1;
					out_cards[mark][pattern + 4] = 1;
					out_cards[mark][pattern + 5] = 1;
					out_cards[mark][pattern + 6] = 1;
				}
			}
		}

		if (qty2 == 8)
		{
			if (pattern < 7)
			{
				if (my_cards[mark][pattern] + my_cards[mark][pattern + 1] + my_cards[mark][pattern + 2] + my_cards[mark][pattern + 3] + my_cards[mark][pattern + 4] + my_cards[mark][pattern + 5] + my_cards[mark][pattern + 6] + my_cards[mark][pattern + 7] == qty2)
				{
					out_cards[mark][pattern] = 1;
					out_cards[mark][pattern + 1] = 1;
					out_cards[mark][pattern + 2] = 1;
					out_cards[mark][pattern + 3] = 1;
					out_cards[mark][pattern + 4] = 1;
					out_cards[mark][pattern + 5] = 1;
					out_cards[mark][pattern + 6] = 1;
					out_cards[mark][pattern + 7] = 1;
				}
			}
		}
	}
	else
	{

		if (qty2 == 3)
		{
			if (pattern < 12)
			{
				if (my_cards[mark][pattern] + my_cards[mark][pattern + 1] + my_cards[mark][pattern + 2] >= qty2 - 1)
				{
					if (my_cards[mark][pattern] == 1)
					{
						out_cards[mark][pattern] = 1;
					}
					else
					{
						out_cards[mark][pattern] = 2;
					}
					if (my_cards[mark][pattern + 1] == 1)
					{
						out_cards[mark][pattern + 1] = 1;
					}
					else
					{
						out_cards[mark][pattern + 1] = 2;
					}
					if (my_cards[mark][pattern + 2] == 1)
					{
						out_cards[mark][pattern + 2] = 1;
					}
					else
					{
						out_cards[mark][pattern + 2] = 2;
					}
				}
			}
		}

		if (qty2 == 4)
		{
			if (pattern < 11)
			{
				if (my_cards[mark][pattern] + my_cards[mark][pattern + 1] + my_cards[mark][pattern + 2] + my_cards[mark][pattern + 3] >= qty2 - 1)
				{
					if (my_cards[mark][pattern] == 1)
					{
						out_cards[mark][pattern] = 1;
					}
					else
					{
						out_cards[mark][pattern] = 2;
					}
					if (my_cards[mark][pattern + 1] == 1)
					{
						out_cards[mark][pattern + 1] = 1;
					}
					else
					{
						out_cards[mark][pattern + 1] = 2;
					}
					if (my_cards[mark][pattern + 2] == 1)
					{
						out_cards[mark][pattern + 2] = 1;
					}
					else
					{
						out_cards[mark][pattern + 2] = 2;
					}
					if (my_cards[mark][pattern + 3] == 1)
					{
						out_cards[mark][pattern + 3] = 1;
					}
					else
					{
						out_cards[mark][pattern + 3] = 2;
					}
				}
			}
		}

		if (qty2 == 5)
		{
			if (pattern < 10)
			{
				if (my_cards[mark][pattern] + my_cards[mark][pattern + 1] + my_cards[mark][pattern + 2] + my_cards[mark][pattern + 3] + my_cards[mark][pattern + 4] >= qty2 - 1)
				{
					if (my_cards[mark][pattern] == 1)
					{
						out_cards[mark][pattern] = 1;
					}
					else
					{
						out_cards[mark][pattern] = 2;
					}
					if (my_cards[mark][pattern + 1] == 1)
					{
						out_cards[mark][pattern + 1] = 1;
					}
					else
					{
						out_cards[mark][pattern + 1] = 2;
					}
					if (my_cards[mark][pattern + 2] == 1)
					{
						out_cards[mark][pattern + 2] = 1;
					}
					else
					{
						out_cards[mark][pattern + 2] = 2;
					}
					if (my_cards[mark][pattern + 3] == 1)
					{
						out_cards[mark][pattern + 3] = 1;
					}
					else
					{
						out_cards[mark][pattern + 3] = 2;
					}
					if (my_cards[mark][pattern + 4] == 1)
					{
						out_cards[mark][pattern + 4] = 1;
					}
					else
					{
						out_cards[mark][pattern + 4] = 2;
					}
				}
			}
		}

		if (qty2 == 6)
		{
			if (pattern < 9)
			{
				if (my_cards[mark][pattern] + my_cards[mark][pattern + 1] + my_cards[mark][pattern + 2] + my_cards[mark][pattern + 3] + my_cards[mark][pattern + 4] + my_cards[mark][pattern + 5] >= qty2 - 1)
				{

					if (my_cards[mark][pattern] == 1)
					{
						out_cards[mark][pattern] = 1;
					}
					else
					{
						out_cards[mark][pattern] = 2;
					}
					if (my_cards[mark][pattern + 1] == 1)
					{
						out_cards[mark][pattern + 1] = 1;
					}
					else
					{
						out_cards[mark][pattern + 1] = 2;
					}
					if (my_cards[mark][pattern + 2] == 1)
					{
						out_cards[mark][pattern + 2] = 1;
					}
					else
					{
						out_cards[mark][pattern + 2] = 2;
					}
					if (my_cards[mark][pattern + 3] == 1)
					{
						out_cards[mark][pattern + 3] = 1;
					}
					else
					{
						out_cards[mark][pattern + 3] = 2;
					}
					if (my_cards[mark][pattern + 4] == 1)
					{
						out_cards[mark][pattern + 4] = 1;
					}
					else
					{
						out_cards[mark][pattern + 4] = 2;
					}
					if (my_cards[mark][pattern + 5] == 1)
					{
						out_cards[mark][pattern + 5] = 1;
					}
					else
					{
						out_cards[mark][pattern + 5] = 2;
					}
				}
			}
		}

		if (qty2 == 7)
		{
			if (pattern < 8)
			{
				if (my_cards[mark][pattern] + my_cards[mark][pattern + 1] + my_cards[mark][pattern + 2] + my_cards[mark][pattern + 3] + my_cards[mark][pattern + 4] + my_cards[mark][pattern + 5] + my_cards[mark][pattern + 6] >= qty2 - 1)
				{

					if (my_cards[mark][pattern] == 1)
					{
						out_cards[mark][pattern] = 1;
					}
					else
					{
						out_cards[mark][pattern] = 2;
					}
					if (my_cards[mark][pattern + 1] == 1)
					{
						out_cards[mark][pattern + 1] = 1;
					}
					else
					{
						out_cards[mark][pattern + 1] = 2;
					}
					if (my_cards[mark][pattern + 2] == 1)
					{
						out_cards[mark][pattern + 2] = 1;
					}
					else
					{
						out_cards[mark][pattern + 2] = 2;
					}
					if (my_cards[mark][pattern + 3] == 1)
					{
						out_cards[mark][pattern + 3] = 1;
					}
					else
					{
						out_cards[mark][pattern + 3] = 2;
					}
					if (my_cards[mark][pattern + 4] == 1)
					{
						out_cards[mark][pattern + 4] = 1;
					}
					else
					{
						out_cards[mark][pattern + 4] = 2;
					}
					if (my_cards[mark][pattern + 5] == 1)
					{
						out_cards[mark][pattern + 5] = 1;
					}
					else
					{
						out_cards[mark][pattern + 5] = 2;
					}
					if (my_cards[mark][pattern + 6] == 1)
					{
						out_cards[mark][pattern + 6] = 1;
					}
					else
					{
						out_cards[mark][pattern + 6] = 2;
					}
				}
			}
		}

		if (qty2 == 8)
		{
			if (pattern < 7)
			{
				if (my_cards[mark][pattern] + my_cards[mark][pattern + 1] + my_cards[mark][pattern + 2] + my_cards[mark][pattern + 3] + my_cards[mark][pattern + 4] + my_cards[mark][pattern + 5] + my_cards[mark][pattern + 6] + my_cards[mark][pattern + 7] >= qty2 - 1)
				{

					if (my_cards[mark][pattern] == 1)
					{
						out_cards[mark][pattern] = 1;
					}
					else
					{
						out_cards[mark][pattern] = 2;
					}
					if (my_cards[mark][pattern + 1] == 1)
					{
						out_cards[mark][pattern + 1] = 1;
					}
					else
					{
						out_cards[mark][pattern + 1] = 2;
					}
					if (my_cards[mark][pattern + 2] == 1)
					{
						out_cards[mark][pattern + 2] = 1;
					}
					else
					{
						out_cards[mark][pattern + 2] = 2;
					}
					if (my_cards[mark][pattern + 3] == 1)
					{
						out_cards[mark][pattern + 3] = 1;
					}
					else
					{
						out_cards[mark][pattern + 3] = 2;
					}
					if (my_cards[mark][pattern + 4] == 1)
					{
						out_cards[mark][pattern + 4] = 1;
					}
					else
					{
						out_cards[mark][pattern + 4] = 2;
					}
					if (my_cards[mark][pattern + 5] == 1)
					{
						out_cards[mark][pattern + 5] = 1;
					}
					else
					{
						out_cards[mark][pattern + 5] = 2;
					}
					if (my_cards[mark][pattern + 6] == 1)
					{
						out_cards[mark][pattern + 6] = 1;
					}
					else
					{
						out_cards[mark][pattern + 6] = 2;
					}
					if (my_cards[mark][pattern + 7] == 1)
					{
						out_cards[mark][pattern + 7] = 1;
					}
					else
					{
						out_cards[mark][pattern + 7] = 2;
					}
				}
			}
		}
	}
}

int nCards2(int n_cards[8][15], int target[8][15], int n)
{
	/*
  */
	int i, j, flag = 0;
	clearTable(n_cards); //テーブルをクリア
	for (i = 0; i < 4; i++)
		for (j = 0; j < 15; j++) //テーブル全体を走査し
			if (target[i][j] >= (int)n)
			{ //n「以上」となるものをみつけたとき
				n_cards[i][j] = n;
				flag = 1; //フラグをたて
			}
			else
			{					   //n以外の場所は
				n_cards[i][j] = 0; //0で埋める。
			}
	return flag;
}

int follow_o(int out_cards[8][15], int my_cards[8][15])
{

	int rest_joker = 0;

	clearTable(out_cards);

	if (my_cards[4][1] == 2)
		rest_joker = 1;

	if (state.qty == 1)
	{
		followSolo_o(out_cards, my_cards, rest_joker); //一枚のとき
	}
	else
	{
		if (state.sequence == 0)
		{
			followGroup_o(out_cards, my_cards, rest_joker); //枚数組のとき
		}
		else
		{
			followSequence_o(out_cards, my_cards, rest_joker); //階段のとき
		}
	}

	if (beEmptyCards(out_cards) == 1)
	{
		return 1; //空なら１を返す
	}
	return 0;
}

void followSolo_o(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	//int group[8][15];
	// int sequence[8][15];
	int temp[8][15];
	/*
  makeGroupTable(group,my_cards);           //枚数組を書き出す
  makeKaidanTable(sequence,my_cards);       //階段を書き出す
  
  removeSequence(temp,my_cards,sequence);   // 階段を除去
  removeGroup(out_cards,temp,group);        // 枚数組を除去
*/

	highCards(temp, my_cards, state.ord); // 場のカードより弱いカードを除去

	if (state.lock == 1)
	{
		lockCards(temp, state.suit); //ロックされているとき出せないカードを除去
	}
	lowSolo(out_cards, temp, joker_flag); //残ったカードから弱いカードを抜き出す
}

void followGroup_o(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int group[8][15];
	int ngroup[8][15];
	int temp[8][15];

	highCards(temp, my_cards, state.ord); //場より強いカードを残す
	if (state.lock == 1)
	{								 //ロックされているとき
		lockCards(temp, state.suit); //出せないカードを除去
	}
	/*
  makeGroupTable(group,temp);                  //残ったものから枚数組を書き出す
  if(nCards2(ngroup,group,state.qty)==0&&joker_flag==1){
    //2
    makeJGroupTable_o(group,temp);               
    nCards2(ngroup,group,state.qty);     //2 
  }
	*/
	if (joker_flag == 0)
	{
		makeGroupTable(group, temp);
		nCards2(ngroup, group, state.qty2);
	}
	else
	{
		makeJGroupTable_o(group, temp);
		nCards2(ngroup, group, state.qty2);
	}
	lowGroup(out_cards, my_cards, ngroup); //一番弱い組を抜き出す
}

void followSequence_o(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int seq[8][15];
	int nseq[8][15];
	int temp[8][15];

	highCards(temp, my_cards, state.ord); //場より強いカードを残す
	if (state.lock == 1)
	{								 //ロックされているとき
		lockCards(temp, state.suit); //出せないカードを除去
	}
	/*
  makeKaidanTable(seq,temp);                   //階段を書き出す
  if(nCards2(nseq,seq,state.qty)==0&&joker_flag==1){
    //2
    makeJKaidanTable_o(seq,temp);
    nCards2(nseq,seq,state.qty);          //2
  }
	*/
	if (joker_flag == 0)
	{
		makeKaidanTable(seq, temp);
		nCards2(nseq, seq, state.qty2);
	}
	else
	{
		makeJKaidanTable_o(seq, temp);
		nCards2(nseq, seq, state.qty2);
	}
	lowSequence(out_cards, my_cards, nseq); //一番弱い階段を
}

void makeJGroupTable_o(int tgt_cards[][15], int my_cards[][15])
{
	/*
    渡されたカードテーブルmy_cardsから、
    ジョーカーを考慮し2枚以上の枚数組で出せるかどうかを解析し、
    結果をテーブルtgt_cardsに格納する。
  */
	int i, j;
	int count;

	clearTable(tgt_cards);
	//if(state.joker!=0){
	for (i = 0; i < 14; i++)
	{ //それそれの強さのカードの枚数を数え ジョーカーの分を加える
		count = my_cards[0][i] + my_cards[1][i] + my_cards[2][i] + my_cards[3][i] + 1;
		if (count > 1)
		{ //枚数が2枚以上のとき
			for (j = 0; j < 4; j++)
			{
				if (my_cards[j][i] == 1)
				{							 //カードを持っている部分に
					tgt_cards[j][i] = count; //その枚数を格納
				}
			}
		}
	}
	//}
	if (g_logging == 1)
	{
		printf("make Joker group \n");
		outputTable(tgt_cards);
	}
}

void makeJKaidanTable_o(int tgt_cards[][15], int my_cards[][15])
{
	/*
    渡されたカードテーブルmy_cardsから、ジョーカーを考慮し階段で出せるかどうかを解析し、
    結果をテーブルtgt_cardsに格納する。
  */
	int i, j;
	int count, noJcount; //ジョーカーを使用した場合のカードの枚数,使用しない枚数

	clearTable(tgt_cards); //テーブルのクリア
						   //if(state.joker==1){            //jokerがあるとき
	for (i = 0; i < 4; i++)
	{ //各スート毎に走査し
		count = 1;
		noJcount = 0;
		for (j = 13; j >= 0; j--)
		{ //順番にみて
			if (my_cards[i][j] == 1)
			{			 //カードがあるとき
				count++; //2つのカウンタを進める
				noJcount++;
			}
			else
			{						  //カードがないとき
				count = noJcount + 1; //ジョーカーありの階段の枚数にジョーカー分を足す
				noJcount = 0;		  //ジョーカーなしの階段の枚数をリセットする
			}

			if (count > 2)
			{							 //3枚以上のとき
				tgt_cards[i][j] = count; //その枚数をテーブルに格納
			}
			else
			{
				tgt_cards[i][j] = 0; //その他は0にする
			}
		}
	}
	//}

	if (g_logging == 1)
	{
		printf("make Joker kaidan \n");
		outputTable(tgt_cards);
	}
}

void getField2(int cards[8][15])
{
	/*
    場に出たカードの情報を得る。
    引数は場に出たカードのテーブル
    情報は広域変数stateに格納される
  */
	int i, j, count = 0;
	int found_joker = 0;
	i = j = 0;

	//カードのある位置を探す
	while (j < 15 && cards[i][j] == 0)
	{
		state.suit2[i] = 0;
		i++;
		if (i == 4)
		{
			j++;
			i = 0;
		}
	}

	//見つけたカードがジョーカーならば、found_joker=1。
	if (cards[i][j] == 2)
	{
		found_joker = 1;
	}

	//階段が否か
	if (j < 14)
	{
		if (cards[i][j + 1] > 0)
			state.sequence2 = 1;
		else
			state.sequence2 = 0;
	}
	//枚数を数える また強さを調べる
	if (state.sequence2 == 0)
	{
		//枚数組
		for (; i < 5; i++)
		{
			if (cards[i][j] > 0)
			{
				count++;
				state.suit2[i] = 1;
			}
			else
			{
				state.suit2[i] = 0;
			}
		}

		//ジョーカー単騎が場に出ているならば、state.ordを最大の強さを示すものに設定
		//ノーマルカードの場合は、その強さをそのままstate.ordへ格納
		if ((found_joker == 1) && (count == 1))
		{
			if (state.rev == 0)
			{
				state.ord2 = 14;
			}
			else
			{
				state.ord2 = 0;
			}
		}
		else
		{
			state.ord2 = j;
		}
	}
	else
	{
		//階段
		while (j + count < 15 && cards[i][j + count] > 0)
		{
			count++;
		}
		if ((state.rev == 0 && state.b11 == 0) || (state.rev == 1 && state.b11 == 1))
		{
			state.ord2 = j + count - 1;
		}
		else
		{
			state.ord2 = j;
		}
		state.suit2[i] = 1;
	}
	//枚数を記憶
	state.qty2 = count;

	/*
  if(state.qty>0){ //枚数が0より大きいとき 新しい場のフラグを0にする
    state.onset=0;
  }
	*/
}

int follow_o_shibari(int out_cards[8][15], int my_cards[8][15])
{

	int rest_joker = 0;

	clearTable(out_cards);

	if (my_cards[4][1] != 0)
		rest_joker = 1;

	if (state.qty2 == 1)
	{
		followSolo_o_shibari(out_cards, my_cards, rest_joker); //一枚のとき
	}
	else
	{
		if (state.sequence2 == 0)
		{
			followGroup_o_shibari(out_cards, my_cards, rest_joker); //枚数組のとき
		}
		else
		{
			followSequence_o_shibari(out_cards, my_cards, rest_joker); //階段のとき
		}
	}

	if (beEmptyCards(out_cards) == 1)
	{
		return 1;
	}
	return 0;
}

void followSolo_o_shibari(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	//int group[8][15];
	// int sequence[8][15];
	int temp[8][15];
	/*
  makeGroupTable(group,my_cards);           //枚数組を書き出す
  makeKaidanTable(sequence,my_cards);       //階段を書き出す
  
  removeSequence(temp,my_cards,sequence);   // 階段を除去
  removeGroup(out_cards,temp,group);        // 枚数組を除去
*/

	highCards(temp, my_cards, state.ord2); // 場のカードより弱いカードを除去

	//if(state.lock==1){ shibari
	lockCards(temp, state.suit2); //ロックされているとき出せないカードを除去
								  //}
								  //lowSolo(out_cards,temp,joker_flag);      //残ったカードから弱いカードを抜き出す
	highSolo(out_cards, temp, joker_flag);
}

void followGroup_o_shibari(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int group[8][15];
	int ngroup[8][15];
	int temp[8][15];

	highCards(temp, my_cards, state.ord2); //場より強いカードを残す
										   // if(state.lock==1){  //shibari                         //ロックされているとき
	lockCards(temp, state.suit2);		   //出せないカードを除去
										   //}
	/*
  makeGroupTable(group,temp);                  //残ったものから枚数組を書き出す
  if(nCards2(ngroup,group,state.qty2)==0&&joker_flag==1){
    //
    makeJGroupTable_o(group,temp);               
    nCards2(ngroup,group,state.qty2);     //2 
  }
	*/
	if (joker_flag == 0)
	{
		makeGroupTable(group, temp);
		nCards2(ngroup, group, state.qty2);
	}
	else
	{
		makeJGroupTable_o(group, temp);
		nCards2(ngroup, group, state.qty2);
	}
	//lowGroup(out_cards,my_cards,ngroup);  //一番弱い組を抜き出す
	highGroup(out_cards, my_cards, ngroup);
}

void followSequence_o_shibari(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int seq[8][15];
	int nseq[8][15];
	int temp[8][15];

	highCards(temp, my_cards, state.ord2); //場より強いカードを残す
										   // if(state.lock==1){  //shibari                         //ロックされているとき
	lockCards(temp, state.suit2);		   //出せないカードを除去
										   //}
	/*
  makeKaidanTable(seq,temp);                   //階段を書き出す
  if(nCards2(nseq,seq,state.qty2)==0&&joker_flag==1){
    //2
    makeJKaidanTable_o(seq,temp);
    nCards2(nseq,seq,state.qty2);          //2
  }
	*/
	if (joker_flag == 0)
	{
		makeKaidanTable(seq, temp);
		nCards2(nseq, seq, state.qty2);
	}
	else
	{
		makeJKaidanTable_o(seq, temp);
		nCards2(nseq, seq, state.qty2);
	}
	//lowSequence(out_cards,my_cards,nseq);  //一番弱い階段を
	highSequence(out_cards, my_cards, nseq);
}

int follow_o2(int out_cards[8][15], int my_cards[8][15])
{

	int rest_joker = 0;

	clearTable(out_cards);

	if (my_cards[4][1] != 0)
		rest_joker = 1;

	if (state.qty2 == 1)
	{
		followSolo_o2(out_cards, my_cards, rest_joker); //一枚のとき
	}
	else
	{
		if (state.sequence2 == 0)
		{
			followGroup_o2(out_cards, my_cards, rest_joker); //枚数組のとき
		}
		else
		{
			followSequence_o2(out_cards, my_cards, rest_joker); //階段のとき
		}
	}

	if (beEmptyCards(out_cards) == 1)
	{
		return 1;
	}
	return 0;
}

void followSolo_o2(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	//int group[8][15];
	// int sequence[8][15];
	int temp[8][15];
	/*
  makeGroupTable(group,my_cards);           //枚数組を書き出す
  makeKaidanTable(sequence,my_cards);       //階段を書き出す
  
  removeSequence(temp,my_cards,sequence);   // 階段を除去
  removeGroup(out_cards,temp,group);        // 枚数組を除去
*/

	highCards(temp, my_cards, state.ord2); // 場のカードより弱いカードを除去

	//if(state.lock==1){ shibari
	//  lockCards(temp,state.suit2);             //ロックされているとき出せないカードを除去
	//}
	//lowSolo(out_cards,temp,joker_flag);      //残ったカードから弱いカードを抜き出す
	highSolo(out_cards, temp, joker_flag);
}

void followGroup_o2(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int group[8][15];
	int ngroup[8][15];
	int temp[8][15];

	highCards(temp, my_cards, state.ord2); //場より強いカードを残す
	// if(state.lock==1){  //shibari                         //ロックされているとき
	//  lockCards(temp,state.suit2);                //出せないカードを除去
	//}
	/*
  makeGroupTable(group,temp);                  //残ったものから枚数組を書き出す
  if(nCards2(ngroup,group,state.qty2)==0&&joker_flag==1){
    //
    makeJGroupTable_o(group,temp);               
    nCards2(ngroup,group,state.qty2);     //2 
  }
	*/

	if (joker_flag == 0)
	{
		makeGroupTable(group, temp);
		nCards2(ngroup, group, state.qty2);
	}
	else
	{
		makeJGroupTable_o(group, temp);
		nCards2(ngroup, group, state.qty2);
	}

	//lowGroup(out_cards,my_cards,ngroup);  //一番弱い組を抜き出す
	highGroup(out_cards, my_cards, ngroup);
}

void followSequence_o2(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int seq[8][15];
	int nseq[8][15];
	int temp[8][15];

	highCards(temp, my_cards, state.ord2); //場より強いカードを残す
	// if(state.lock==1){  //shibari                         //ロックされているとき
	// lockCards(temp,state.suit2);                //出せないカードを除去
	//}
	/*
  makeKaidanTable(seq,temp);                   //階段を書き出す
  if(nCards2(nseq,seq,state.qty2)==0&&joker_flag==1){
    //2
    makeJKaidanTable_o(seq,temp);
    nCards2(nseq,seq,state.qty2);          //2
  }
	*/
	if (joker_flag == 0)
	{
		makeKaidanTable(seq, temp);
		nCards2(nseq, seq, state.qty2);
	}
	else
	{
		makeJKaidanTable_o(seq, temp);
		nCards2(nseq, seq, state.qty2);
	}
	//lowSequence(out_cards,my_cards,nseq);  //一番弱い階段を
	highSequence(out_cards, my_cards, nseq);
}

int follow_o3_shibari(int out_cards[8][15], int my_cards[8][15])
{

	int rest_joker = 0;

	clearTable(out_cards);

	if (my_cards[4][1] != 0)
		rest_joker = 1;

	if (state.qty2 == 1)
	{
		followSolo_o3_shibari(out_cards, my_cards, rest_joker); //一枚のとき
	}
	else
	{
		if (state.sequence2 == 0)
		{
			followGroup_o3_shibari(out_cards, my_cards, rest_joker); //枚数組のとき
		}
		else
		{
			followSequence_o3_shibari(out_cards, my_cards, rest_joker); //階段のとき
		}
	}

	if (beEmptyCards(out_cards) == 1)
	{
		return 1;
	}
	return 0;
}

void followSolo_o3_shibari(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	//int group[8][15];
	// int sequence[8][15];
	int temp[8][15];
	/*
  makeGroupTable(group,my_cards);           //枚数組を書き出す
  makeKaidanTable(sequence,my_cards);       //階段を書き出す
  
  removeSequence(temp,my_cards,sequence);   // 階段を除去
  removeGroup(out_cards,temp,group);        // 枚数組を除去
*/

	highCards(temp, my_cards, state.ord2); // 場のカードより弱いカードを除去

	//if(state.lock==1){ shibari
	lockCards(temp, state.suit2); //ロックされているとき出せないカードを除去
	//}
	lowSolo(out_cards, temp, joker_flag); //残ったカードから弱いカードを抜き出す
										  //highSolo(out_cards,temp,joker_flag);
}

void followGroup_o3_shibari(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int group[8][15];
	int ngroup[8][15];
	int temp[8][15];

	highCards(temp, my_cards, state.ord2); //場より強いカードを残す
										   // if(state.lock==1){  //shibari                         //ロックされているとき
	lockCards(temp, state.suit2);		   //出せないカードを除去
										   //}
	/*
  makeGroupTable(group,temp);                  //残ったものから枚数組を書き出す
  if(nCards2(ngroup,group,state.qty2)==0&&joker_flag==1){
    //
    makeJGroupTable_o(group,temp);               
    nCards2(ngroup,group,state.qty2);     //2 
  }
	*/
	if (joker_flag == 0)
	{
		makeGroupTable(group, temp);
		nCards2(ngroup, group, state.qty2);
	}
	else
	{
		makeJGroupTable_o(group, temp);
		nCards2(ngroup, group, state.qty2);
	}
	lowGroup(out_cards, my_cards, ngroup); //一番弱い組を抜き出す
										   //highGroup(out_cards,my_cards,ngroup);
}

void followSequence_o3_shibari(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int seq[8][15];
	int nseq[8][15];
	int temp[8][15];

	highCards(temp, my_cards, state.ord2); //場より強いカードを残す
										   // if(state.lock==1){  //shibari                         //ロックされているとき
	lockCards(temp, state.suit2);		   //出せないカードを除去
										   //}
	/*
  makeKaidanTable(seq,temp);                   //階段を書き出す
  if(nCards2(nseq,seq,state.qty2)==0&&joker_flag==1){
    //2
    makeJKaidanTable_o(seq,temp);
    nCards2(nseq,seq,state.qty2);          //2
  }
	*/
	if (joker_flag == 0)
	{
		makeKaidanTable(seq, temp);
		nCards2(nseq, seq, state.qty2);
	}
	else
	{
		makeJKaidanTable_o(seq, temp);
		nCards2(nseq, seq, state.qty2);
	}
	lowSequence(out_cards, my_cards, nseq); //一番弱い階段を
											//highSequence(out_cards,my_cards,nseq);
}

int follow_o3(int out_cards[8][15], int my_cards[8][15])
{

	int rest_joker = 0;

	clearTable(out_cards);

	if (my_cards[4][1] != 0)
		rest_joker = 1;

	if (state.qty2 == 1)
	{
		followSolo_o3(out_cards, my_cards, rest_joker); //一枚のとき
	}
	else
	{
		if (state.sequence2 == 0)
		{
			followGroup_o3(out_cards, my_cards, rest_joker); //枚数組のとき
		}
		else
		{
			followSequence_o3(out_cards, my_cards, rest_joker); //階段のとき
		}
	}

	if (beEmptyCards(out_cards) == 1)
	{
		return 1;
	}
	return 0;
}

void followSolo_o3(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	//int group[8][15];
	// int sequence[8][15];
	int temp[8][15];
	/*
  makeGroupTable(group,my_cards);           //枚数組を書き出す
  makeKaidanTable(sequence,my_cards);       //階段を書き出す
  
  removeSequence(temp,my_cards,sequence);   // 階段を除去
  removeGroup(out_cards,temp,group);        // 枚数組を除去
*/

	highCards(temp, my_cards, state.ord2); // 場のカードより弱いカードを除去

	//if(state.lock==1){ shibari
	//  lockCards(temp,state.suit2);             //ロックされているとき出せないカードを除去
	//}
	lowSolo(out_cards, temp, joker_flag); //残ったカードから弱いカードを抜き出す
										  //highSolo(out_cards,temp,joker_flag);
}

void followGroup_o3(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int group[8][15];
	int ngroup[8][15];
	int temp[8][15];

	highCards(temp, my_cards, state.ord2); //場より強いカードを残す
	// if(state.lock==1){  //shibari                         //ロックされているとき
	//  lockCards(temp,state.suit2);                //出せないカードを除去
	//}
	/*
  makeGroupTable(group,temp);                  //残ったものから枚数組を書き出す
  if(nCards2(ngroup,group,state.qty2)==0&&joker_flag==1){
    //
    makeJGroupTable_o(group,temp);               
    nCards2(ngroup,group,state.qty2);     //2 
  }
	*/

	if (joker_flag == 0)
	{
		makeGroupTable(group, temp);
		nCards2(ngroup, group, state.qty2);
	}
	else
	{
		makeJGroupTable_o(group, temp);
		nCards2(ngroup, group, state.qty2);
	}

	lowGroup(out_cards, my_cards, ngroup); //一番弱い組を抜き出す
										   //highGroup(out_cards,my_cards,ngroup);
}

void followSequence_o3(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int seq[8][15];
	int nseq[8][15];
	int temp[8][15];

	highCards(temp, my_cards, state.ord2); //場より強いカードを残す
	// if(state.lock==1){  //shibari                         //ロックされているとき
	// lockCards(temp,state.suit2);                //出せないカードを除去
	//}
	/*
  makeKaidanTable(seq,temp);                   //階段を書き出す
  if(nCards2(nseq,seq,state.qty2)==0&&joker_flag==1){
    //2
    makeJKaidanTable_o(seq,temp);
    nCards2(nseq,seq,state.qty2);          //2
  }
	*/
	if (joker_flag == 0)
	{
		makeKaidanTable(seq, temp);
		nCards2(nseq, seq, state.qty2);
	}
	else
	{
		makeJKaidanTable_o(seq, temp);
		nCards2(nseq, seq, state.qty2);
	}
	lowSequence(out_cards, my_cards, nseq); //一番弱い階段を
											//highSequence(out_cards,my_cards,nseq);
}

int follow_o_rev(int out_cards[8][15], int my_cards[8][15])
{

	int rest_joker = 0;

	clearTable(out_cards);

	if (my_cards[4][1] == 2)
		rest_joker = 1;

	if (state.qty == 1)
	{
		followSolo_o_rev(out_cards, my_cards, rest_joker); //一枚のとき
	}
	else
	{
		if (state.sequence == 0)
		{
			followGroup_o_rev(out_cards, my_cards, rest_joker); //枚数組のとき
		}
		else
		{
			followSequence_o(out_cards, my_cards, rest_joker); //階段のとき
		}
	}

	if (beEmptyCards(out_cards) == 1)
	{
		return 1;
	}
	return 0;
}

void followSolo_o_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	//int group[8][15];
	// int sequence[8][15];
	int temp[8][15];
	/*
  makeGroupTable(group,my_cards);           //枚数組を書き出す
  makeKaidanTable(sequence,my_cards);       //階段を書き出す
  
  removeSequence(temp,my_cards,sequence);   // 階段を除去
  removeGroup(out_cards,temp,group);        // 枚数組を除去
*/

	lowCards(temp, my_cards, state.ord); //

	if (state.lock == 1)
	{
		lockCards(temp, state.suit); //ロックされているとき出せないカードを除去
	}
	highSolo(out_cards, temp, joker_flag); //残ったカードから弱いカードを抜き出す
}

void followGroup_o_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int group[8][15];
	int ngroup[8][15];
	int temp[8][15];

	lowCards(temp, my_cards, state.ord); //
	if (state.lock == 1)
	{								 //ロックされているとき
		lockCards(temp, state.suit); //出せないカードを除去
	}								 /*
  makeGroupTable(group,temp);                  //残ったものから枚数組を書き出す
  if(nCards2(ngroup,group,state.qty)==0&&joker_flag==1){
    //2
    makeJGroupTable_o(group,temp);               
    nCards2(ngroup,group,state.qty);     //2 
  }*/
	if (joker_flag == 0)
	{
		makeGroupTable(group, temp);
		nCards2(ngroup, group, state.qty2);
	}
	else
	{
		makeJGroupTable_o(group, temp);
		nCards2(ngroup, group, state.qty2);
	}

	highGroup(out_cards, my_cards, ngroup); //一番弱い組を抜き出す
}

void followSequence_o_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int seq[8][15];
	int nseq[8][15];
	int temp[8][15];

	lowCards(temp, my_cards, state.ord); //
	if (state.lock == 1)
	{								 //ロックされているとき
		lockCards(temp, state.suit); //出せないカードを除去
	}
	/*
  makeKaidanTable(seq,temp);                   //階段を書き出す
  if(nCards2(nseq,seq,state.qty)==0&&joker_flag==1){
    //2
    makeJKaidanTable_o(seq,temp);
    nCards2(nseq,seq,state.qty);          //2
  }*/

	if (joker_flag == 0)
	{
		makeKaidanTable(seq, temp);
		nCards2(nseq, seq, state.qty2);
	}
	else
	{
		makeJKaidanTable_o(seq, temp);
		nCards2(nseq, seq, state.qty2);
	}

	highSequence(out_cards, my_cards, nseq); //一番弱い階段を
}

int follow_o_shibari_rev(int out_cards[8][15], int my_cards[8][15])
{

	int rest_joker = 0;

	clearTable(out_cards);

	if (my_cards[4][1] != 0)
		rest_joker = 1;

	if (state.qty2 == 1)
	{
		followSolo_o_shibari_rev(out_cards, my_cards, rest_joker); //一枚のとき
	}
	else
	{
		if (state.sequence2 == 0)
		{
			followGroup_o_shibari_rev(out_cards, my_cards, rest_joker); //枚数組のとき
		}
		else
		{
			followSequence_o_shibari_rev(out_cards, my_cards, rest_joker); //階段のとき
		}
	}

	if (beEmptyCards(out_cards) == 1)
	{
		return 1;
	}
	return 0;
}

void followSolo_o_shibari_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	//int group[8][15];
	// int sequence[8][15];
	int temp[8][15];
	/*
  makeGroupTable(group,my_cards);           //枚数組を書き出す
  makeKaidanTable(sequence,my_cards);       //階段を書き出す
  
  removeSequence(temp,my_cards,sequence);   // 階段を除去
  removeGroup(out_cards,temp,group);        // 枚数組を除去
*/

	lowCards(temp, my_cards, state.ord2); //

	//if(state.lock==1){ shibari
	lockCards(temp, state.suit2); //ロックされているとき出せないカードを除去
	//}
	lowSolo(out_cards, temp, joker_flag); //残ったカードから弱いカードを抜き出す
										  //highSolo(out_cards,temp,joker_flag);
}

void followGroup_o_shibari_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int group[8][15];
	int ngroup[8][15];
	int temp[8][15];

	lowCards(temp, my_cards, state.ord2); //
										  // if(state.lock==1){  //shibari                         //ロックされているとき
	lockCards(temp, state.suit2);		  //出せないカードを除去
										  //}
	/*
  makeGroupTable(group,temp);                  //残ったものから枚数組を書き出す
  if(nCards2(ngroup,group,state.qty2)==0&&joker_flag==1){
    //
    makeJGroupTable_o(group,temp);               
    nCards2(ngroup,group,state.qty2);     //2 
  }*/

	if (joker_flag == 0)
	{
		makeGroupTable(group, temp);
		nCards2(ngroup, group, state.qty2);
	}
	else
	{
		makeJGroupTable_o(group, temp);
		nCards2(ngroup, group, state.qty2);
	}

	lowGroup(out_cards, my_cards, ngroup); //一番ランクの小さい組を抜き出す
										   //highGroup(out_cards,my_cards,ngroup);
}

void followSequence_o_shibari_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int seq[8][15];
	int nseq[8][15];
	int temp[8][15];

	lowCards(temp, my_cards, state.ord2); //
										  // if(state.lock==1){  //shibari                         //ロックされているとき
	lockCards(temp, state.suit2);		  //出せないカードを除去
										  //}
	/*
  makeKaidanTable(seq,temp);                   //階段を書き出す
  if(nCards2(nseq,seq,state.qty2)==0&&joker_flag==1){
    //2
    makeJKaidanTable_o(seq,temp);
    nCards2(nseq,seq,state.qty2);          //2
  }*/

	if (joker_flag == 0)
	{
		makeKaidanTable(seq, temp);
		nCards2(nseq, seq, state.qty2);
	}
	else
	{
		makeJKaidanTable_o(seq, temp);
		nCards2(nseq, seq, state.qty2);
	}
	lowSequence(out_cards, my_cards, nseq); //一番ランクの小さい階段を
											//highSequence(out_cards,my_cards,nseq);
}

int follow_o2_rev(int out_cards[8][15], int my_cards[8][15])
{

	int rest_joker = 0;

	clearTable(out_cards);

	if (my_cards[4][1] != 0)
		rest_joker = 1;

	if (state.qty2 == 1)
	{
		followSolo_o2_rev(out_cards, my_cards, rest_joker); //一枚のとき
	}
	else
	{
		if (state.sequence2 == 0)
		{
			followGroup_o2_rev(out_cards, my_cards, rest_joker); //枚数組のとき
		}
		else
		{
			followSequence_o2_rev(out_cards, my_cards, rest_joker); //階段のとき
		}
	}

	if (beEmptyCards(out_cards) == 1)
	{
		return 1;
	}
	return 0;
}

void followSolo_o2_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	//int group[8][15];
	// int sequence[8][15];
	int temp[8][15];
	/*
  makeGroupTable(group,my_cards);           //枚数組を書き出す
  makeKaidanTable(sequence,my_cards);       //階段を書き出す
  
  removeSequence(temp,my_cards,sequence);   // 階段を除去
  removeGroup(out_cards,temp,group);        // 枚数組を除去
*/

	lowCards(temp, my_cards, state.ord2); //

	//if(state.lock==1){ shibari
	//  lockCards(temp,state.suit2);             //ロックされているとき出せないカードを除去
	//}
	//lowSolo(out_cards,temp,joker_flag);

	lowSolo(out_cards, temp, joker_flag); //ランクが小さいカードを探す。
}

void followGroup_o2_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int group[8][15];
	int ngroup[8][15];
	int temp[8][15];

	lowCards(temp, my_cards, state.ord2); //
	// if(state.lock==1){  //shibari                         //ロックされているとき
	//  lockCards(temp,state.suit2);                //出せないカードを除去
	//}
	/*
  makeGroupTable(group,temp);                  //残ったものから枚数組を書き出す
  if(nCards2(ngroup,group,state.qty2)==0&&joker_flag==1){
    //
    makeJGroupTable_o(group,temp);               
    nCards2(ngroup,group,state.qty2);     //2 
  }
  //lowGroup(out_cards,my_cards,ngroup);
	*/

	if (joker_flag == 0)
	{
		makeGroupTable(group, temp);
		nCards2(ngroup, group, state.qty2);
	}
	else
	{
		makeJGroupTable_o(group, temp);
		nCards2(ngroup, group, state.qty2);
	}

	lowGroup(out_cards, my_cards, ngroup); //ランクが小さいカードを探す。
}

void followSequence_o2_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int seq[8][15];
	int nseq[8][15];
	int temp[8][15];

	lowCards(temp, my_cards, state.ord2); //
	// if(state.lock==1){  //shibari                         //ロックされているとき
	// lockCards(temp,state.suit2);                //出せないカードを除去
	//}
	/*
  makeKaidanTable(seq,temp);                   //階段を書き出す
  if(nCards2(nseq,seq,state.qty2)==0&&joker_flag==1){
    //2
    makeJKaidanTable_o(seq,temp);
    nCards2(nseq,seq,state.qty2);          //2
  }
	*/

	if (joker_flag == 0)
	{
		makeKaidanTable(seq, temp);
		nCards2(nseq, seq, state.qty2);
	}
	else
	{
		makeJKaidanTable_o(seq, temp);
		nCards2(nseq, seq, state.qty2);
	}

	lowSequence(out_cards, my_cards, nseq); //ランクが小さいカードを探す。
}

int follow_o3_shibari_rev(int out_cards[8][15], int my_cards[8][15])
{

	int rest_joker = 0;

	clearTable(out_cards);

	if (my_cards[4][1] != 0)
		rest_joker = 1;

	if (state.qty2 == 1)
	{
		followSolo_o3_shibari_rev(out_cards, my_cards, rest_joker); //一枚のとき
	}
	else
	{
		if (state.sequence2 == 0)
		{
			followGroup_o3_shibari_rev(out_cards, my_cards, rest_joker); //枚数組のとき
		}
		else
		{
			followSequence_o3_shibari_rev(out_cards, my_cards, rest_joker); //階段のとき
		}
	}

	if (beEmptyCards(out_cards) == 1)
	{
		return 1;
	}
	return 0;
}

void followSolo_o3_shibari_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	//int group[8][15];
	// int sequence[8][15];
	int temp[8][15];
	/*
  makeGroupTable(group,my_cards);           //枚数組を書き出す
  makeKaidanTable(sequence,my_cards);       //階段を書き出す
  
  removeSequence(temp,my_cards,sequence);   // 階段を除去
  removeGroup(out_cards,temp,group);        // 枚数組を除去
*/

	lowCards(temp, my_cards, state.ord2); //

	//if(state.lock==1){ shibari
	lockCards(temp, state.suit2); //ロックされているとき出せないカードを除去
								  //}
								  //lowSolo(out_cards,temp,joker_flag);      //残ったカードから弱いカードを抜き出す
	highSolo(out_cards, temp, joker_flag);
}

void followGroup_o3_shibari_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int group[8][15];
	int ngroup[8][15];
	int temp[8][15];

	lowCards(temp, my_cards, state.ord2); //
										  // if(state.lock==1){  //shibari                         //ロックされているとき
	lockCards(temp, state.suit2);		  //出せないカードを除去
										  //}
	/*
  makeGroupTable(group,temp);                  //残ったものから枚数組を書き出す
  if(nCards2(ngroup,group,state.qty2)==0&&joker_flag==1){
    //
    makeJGroupTable_o(group,temp);               
    nCards2(ngroup,group,state.qty2);     //2 
  }*/

	if (joker_flag == 0)
	{
		makeGroupTable(group, temp);
		nCards2(ngroup, group, state.qty2);
	}
	else
	{
		makeJGroupTable_o(group, temp);
		nCards2(ngroup, group, state.qty2);
	}

	//lowGroup(out_cards,my_cards,ngroup);  //一番ランクの小さい組を抜き出す
	highGroup(out_cards, my_cards, ngroup);
}

void followSequence_o3_shibari_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int seq[8][15];
	int nseq[8][15];
	int temp[8][15];

	lowCards(temp, my_cards, state.ord2); //
										  // if(state.lock==1){  //shibari                         //ロックされているとき
	lockCards(temp, state.suit2);		  //出せないカードを除去
										  //}
	/*
  makeKaidanTable(seq,temp);                   //階段を書き出す
  if(nCards2(nseq,seq,state.qty2)==0&&joker_flag==1){
    //2
    makeJKaidanTable_o(seq,temp);
    nCards2(nseq,seq,state.qty2);          //2
  }*/

	if (joker_flag == 0)
	{
		makeKaidanTable(seq, temp);
		nCards2(nseq, seq, state.qty2);
	}
	else
	{
		makeJKaidanTable_o(seq, temp);
		nCards2(nseq, seq, state.qty2);
	}
	//lowSequence(out_cards,my_cards,nseq);  //一番ランクの小さい階段を
	highSequence(out_cards, my_cards, nseq);
}

int follow_o3_rev(int out_cards[8][15], int my_cards[8][15])
{

	int rest_joker = 0;

	clearTable(out_cards);

	if (my_cards[4][1] != 0)
		rest_joker = 1;

	if (state.qty2 == 1)
	{
		followSolo_o3_rev(out_cards, my_cards, rest_joker); //一枚のとき
	}
	else
	{
		if (state.sequence2 == 0)
		{
			followGroup_o3_rev(out_cards, my_cards, rest_joker); //枚数組のとき
		}
		else
		{
			followSequence_o3_rev(out_cards, my_cards, rest_joker); //階段のとき
		}
	}

	if (beEmptyCards(out_cards) == 1)
	{
		return 1;
	}
	return 0;
}

void followSolo_o3_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	//int group[8][15];
	// int sequence[8][15];
	int temp[8][15];
	/*
  makeGroupTable(group,my_cards);           //枚数組を書き出す
  makeKaidanTable(sequence,my_cards);       //階段を書き出す
  
  removeSequence(temp,my_cards,sequence);   // 階段を除去
  removeGroup(out_cards,temp,group);        // 枚数組を除去
*/

	lowCards(temp, my_cards, state.ord2); //

	//if(state.lock==1){ shibari
	//  lockCards(temp,state.suit2);             //ロックされているとき出せないカードを除去
	//}
	//lowSolo(out_cards,temp,joker_flag);

	highSolo(out_cards, temp, joker_flag); //ランクが小さいカードを探す。
}

void followGroup_o3_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int group[8][15];
	int ngroup[8][15];
	int temp[8][15];

	lowCards(temp, my_cards, state.ord2); //
	// if(state.lock==1){  //shibari                         //ロックされているとき
	//  lockCards(temp,state.suit2);                //出せないカードを除去
	//}
	/*
  makeGroupTable(group,temp);                  //残ったものから枚数組を書き出す
  if(nCards2(ngroup,group,state.qty2)==0&&joker_flag==1){
    //
    makeJGroupTable_o(group,temp);               
    nCards2(ngroup,group,state.qty2);     //2 
  }
  //lowGroup(out_cards,my_cards,ngroup);
	*/

	if (joker_flag == 0)
	{
		makeGroupTable(group, temp);
		nCards2(ngroup, group, state.qty2);
	}
	else
	{
		makeJGroupTable_o(group, temp);
		nCards2(ngroup, group, state.qty2);
	}

	highGroup(out_cards, my_cards, ngroup); //ランクが小さいカードを探す。
}

void followSequence_o3_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int seq[8][15];
	int nseq[8][15];
	int temp[8][15];

	lowCards(temp, my_cards, state.ord2); //
	// if(state.lock==1){  //shibari                         //ロックされているとき
	// lockCards(temp,state.suit2);                //出せないカードを除去
	//}
	/*
  makeKaidanTable(seq,temp);                   //階段を書き出す
  if(nCards2(nseq,seq,state.qty2)==0&&joker_flag==1){
    //2
    makeJKaidanTable_o(seq,temp);
    nCards2(nseq,seq,state.qty2);          //2
  }
	*/

	if (joker_flag == 0)
	{
		makeKaidanTable(seq, temp);
		nCards2(nseq, seq, state.qty2);
	}
	else
	{
		makeJKaidanTable_o(seq, temp);
		nCards2(nseq, seq, state.qty2);
	}

	highSequence(out_cards, my_cards, nseq); //ランクが小さいカードを探す。
}

void my_lead(int out_cards[8][15], int my_cards[8][15])
{

	int kuminumber = 0;

	bunkatsu_pre2(my_cards);

	kuminumber = min_kumi_info_kakou(3);

	/*printf("kuminumber:%d\n",kuminumber);
	
	if(kuminumber==0)outputTable(state.kumi0);
	else if(kuminumber==1)outputTable(state.kumi1);
	else if(kuminumber==2)outputTable(state.kumi2);
	else if(kuminumber==3)outputTable(state.kumi3);
	else if(kuminumber==4)outputTable(state.kumi4);
	else if(kuminumber==5)outputTable(state.kumi5);
	else if(kuminumber==6)outputTable(state.kumi6);
	else if(kuminumber==7)outputTable(state.kumi7);
	else if(kuminumber==8)outputTable(state.kumi8);
	else if(kuminumber==9)outputTable(state.kumi9);
	else if(kuminumber==10)outputTable(state.kumi10);*/

	if (kuminumber == 0)
		copyTable(out_cards, state.kumi0);
	else if (kuminumber == 1)
		copyTable(out_cards, state.kumi1);
	else if (kuminumber == 2)
		copyTable(out_cards, state.kumi2);
	else if (kuminumber == 3)
		copyTable(out_cards, state.kumi3);
	else if (kuminumber == 4)
		copyTable(out_cards, state.kumi4);
	else if (kuminumber == 5)
		copyTable(out_cards, state.kumi5);
	else if (kuminumber == 6)
		copyTable(out_cards, state.kumi6);
	else if (kuminumber == 7)
		copyTable(out_cards, state.kumi7);
	else if (kuminumber == 8)
		copyTable(out_cards, state.kumi8);
	else if (kuminumber == 9)
		copyTable(out_cards, state.kumi9);
	else if (kuminumber == 10)
		copyTable(out_cards, state.kumi10);
}

void my_follow(int out_cards[8][15], int my_cards[8][15])
{
	/*
    他のプレーヤーに続いてカードを出すときのルーチン
    場の状態stateに応じて一枚、枚数組、階段の場合に分けて
    対応すれる関数を呼び出す
    提出するカードはカードテーブルout_cardsに格納される
  */

	clearTable(out_cards);

	bunkatsu_pre2(my_cards);

	if (state.qty == 1)
	{
		//followSolo(out_cards,my_cards,state.joker);    //一枚のとき
		my_followSolo(out_cards, my_cards, state.joker); //一枚のとき
	}
	else
	{
		if (state.sequence == 0)
		{
			my_followGroup(out_cards, my_cards, state.joker); //枚数組のとき
		}
		else
		{
			my_followSequence(out_cards, my_cards, state.joker); //階段のとき
		}
	}
}

void my_followSolo(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int temp[8][15] = {{0}};
	int i = 0, rank = 102, suit = 0, set = 3, tsuyosa2 = 120, tsuyosa3 = 120, kumi_suit = 20, tsuyosa4 = 120;

	if (state.ord == 14 && my_cards[0][1] == 1)
		temp[0][1] = 1; //JOKER単体に対する対応（スペ3返し）

	if (beEmptyCards(temp) == 1)
	{ //JOKERに対する対応以外

		if (state.lock == 1)
		{
			for (i = 0; i < 4; i++)
			{
				if (state.suit[i] == 1)
					suit = i;
			}
		}

		if (state.lock == 1)
		{ //ロックされているなら
			for (i = 0; i < 11; i++)
			{
				if (state.kumi_info[i][2] == suit || state.kumi_info[i][2] == 4)
				{ //マークの判定
					if (state.kumi_info[i][1] == state.qty && state.kumi_info[i][3] > state.ord && state.kumi_info[i][set] <= rank)
					{

						if (i == 0)
							copyTable(temp, state.kumi0);
						else if (i == 1)
							copyTable(temp, state.kumi1);
						else if (i == 2)
							copyTable(temp, state.kumi2);
						else if (i == 3)
							copyTable(temp, state.kumi3);
						else if (i == 4)
							copyTable(temp, state.kumi4);
						else if (i == 5)
							copyTable(temp, state.kumi5);
						else if (i == 6)
							copyTable(temp, state.kumi6);
						else if (i == 7)
							copyTable(temp, state.kumi7);
						else if (i == 8)
							copyTable(temp, state.kumi8);
						else if (i == 9)
							copyTable(temp, state.kumi9);
						else if (i == 10)
							copyTable(temp, state.kumi10);

						rank = state.kumi_info[i][set];
					}
				}
			}
		}
		else
		{ //ロックされていないなら
			for (i = 0; i < 11; i++)
			{
				if (state.kumi_info[i][1] == state.qty && state.kumi_info[i][3] > state.ord && state.kumi_info[i][set] <= rank)
				{

					if (i == 0)
						copyTable(temp, state.kumi0);
					else if (i == 1)
						copyTable(temp, state.kumi1);
					else if (i == 2)
						copyTable(temp, state.kumi2);
					else if (i == 3)
						copyTable(temp, state.kumi3);
					else if (i == 4)
						copyTable(temp, state.kumi4);
					else if (i == 5)
						copyTable(temp, state.kumi5);
					else if (i == 6)
						copyTable(temp, state.kumi6);
					else if (i == 7)
						copyTable(temp, state.kumi7);
					else if (i == 8)
						copyTable(temp, state.kumi8);
					else if (i == 9)
						copyTable(temp, state.kumi9);
					else if (i == 10)
						copyTable(temp, state.kumi10);

					rank = state.kumi_info[i][set];

					//printf("win=1\n");
					//if(state.kumi_info[i][4]==state.kumi_info[13][21])printf("rank \n");
				}
			}
		}
	}

	copyTable(out_cards, temp);
}

void my_followGroup(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{
	/*  
    他のプレーヤーに続いてカードを枚数組で出すときのルーチン
    joker_flagが1の時ジョーカーを使おうとする
    提出するカードはカードテーブルout_cardsに格納される
  
  int group[8][15];
  int ngroup[8][15];
  int temp[8][15];
  
  highCards(temp,my_cards,state.ord);          //場より強いカードを残す 
  if(state.lock==1){                           //ロックされているとき
    lockCards(temp,state.suit);                //出せないカードを除去
  }
  makeGroupTable(group,temp);                  //残ったものから枚数組を書き出す
  if(nCards(ngroup,group,state.qty)==0&&state.joker==1){
    //場と同じ枚数の組が無いときジョーカーを使って探す
    makeJGroupTable(group,temp);               
    nCards(ngroup,group,state.qty);     //場と同じ枚数の組のみのこす。 
  }
  lowGroup(out_cards,my_cards,ngroup);  //一番弱い組を抜き出す
	*/

	int temp[8][15] = {{0}};
	int i = 0, rank = 102, suitsum = 0, set = 3;

	if (state.lock == 1)
	{
		for (i = 0; i < 4; i++)
		{
			if (state.suit[i] == 1) //マークを1,2,4,8の組み合わせで表すことにした。
				if (i == 0)
					suitsum = suitsum + 1;
			if (i == 1)
				suitsum = suitsum + 2;
			if (i == 2)
				suitsum = suitsum + 4;
			if (i == 3)
				suitsum = suitsum + 8;
		}
	}

	if (state.lock == 1)
	{ //ロックされているなら
		for (i = 0; i < 11; i++)
		{
			if (state.kumi_info[i][2] == suitsum)
			{ //マークの判定
				if (state.kumi_info[i][1] == state.qty && state.kumi_info[i][3] > state.ord && state.kumi_info[i][set] <= rank)
				{

					if (i == 0)
						copyTable(temp, state.kumi0);
					else if (i == 1)
						copyTable(temp, state.kumi1);
					else if (i == 2)
						copyTable(temp, state.kumi2);
					else if (i == 3)
						copyTable(temp, state.kumi3);
					else if (i == 4)
						copyTable(temp, state.kumi4);
					else if (i == 5)
						copyTable(temp, state.kumi5);
					else if (i == 6)
						copyTable(temp, state.kumi6);
					else if (i == 7)
						copyTable(temp, state.kumi7);
					else if (i == 8)
						copyTable(temp, state.kumi8);
					else if (i == 9)
						copyTable(temp, state.kumi9);
					else if (i == 10)
						copyTable(temp, state.kumi10);

					rank = state.kumi_info[i][set];
				}
			}
		}
	}
	else
	{ //ロックされていないなら
		for (i = 0; i < 11; i++)
		{
			if (state.kumi_info[i][1] == state.qty && state.kumi_info[i][3] > state.ord && state.kumi_info[i][set] <= rank)
			{

				if (i == 0)
					copyTable(temp, state.kumi0);
				else if (i == 1)
					copyTable(temp, state.kumi1);
				else if (i == 2)
					copyTable(temp, state.kumi2);
				else if (i == 3)
					copyTable(temp, state.kumi3);
				else if (i == 4)
					copyTable(temp, state.kumi4);
				else if (i == 5)
					copyTable(temp, state.kumi5);
				else if (i == 6)
					copyTable(temp, state.kumi6);
				else if (i == 7)
					copyTable(temp, state.kumi7);
				else if (i == 8)
					copyTable(temp, state.kumi8);
				else if (i == 9)
					copyTable(temp, state.kumi9);
				else if (i == 10)
					copyTable(temp, state.kumi10);

				rank = state.kumi_info[i][set];
			}
		}
	}

	copyTable(out_cards, temp);
}

void my_followSequence(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{
	/*
    他のプレーヤーに続いてカードを階段で出すときのルーチン
    joker_flagが1の時ジョーカーを使おうとする
    提出するカードはカードテーブルout_cardsに格納される
  
  int seq[8][15];
  int nseq[8][15];
  int temp[8][15];
  
  highCards(temp,my_cards,state.ord);          //場より強いカードを残す
  if(state.lock==1){                           //ロックされているとき
    lockCards(temp,state.suit);                //出せないカードを除去
  }
  makeKaidanTable(seq,temp);                   //階段を書き出す
  if(nCards(nseq,seq,state.qty)==0&&state.joker==1){
    //場と同じ枚数の階段が無いときジョーカーを使って探す
    makeJKaidanTable(seq,temp);
    nCards(nseq,seq,state.qty);          //場と同じ枚数の組のみのこす。
  }
  lowSequence(out_cards,my_cards,nseq);  //一番弱い階段を
	
	*/
	int temp[8][15] = {{0}};
	int i = 0, rank = 102, suit = 0, set = 3;

	if (state.lock == 1)
	{
		for (i = 0; i < 4; i++)
		{
			if (state.suit[i] == 1)
				suit = i;
		}
	}

	if (state.lock == 1)
	{ //ロックされているなら
		for (i = 0; i < 11; i++)
		{
			if (state.kumi_info[i][2] == suit || state.kumi_info[i][2] == 4)
			{ //マークの判定
				if (state.kumi_info[i][1] == state.qty && state.kumi_info[i][3] > state.ord && state.kumi_info[i][set] <= rank)
				{

					if (i == 0)
						copyTable(temp, state.kumi0);
					else if (i == 1)
						copyTable(temp, state.kumi1);
					else if (i == 2)
						copyTable(temp, state.kumi2);
					else if (i == 3)
						copyTable(temp, state.kumi3);
					else if (i == 4)
						copyTable(temp, state.kumi4);
					else if (i == 5)
						copyTable(temp, state.kumi5);
					else if (i == 6)
						copyTable(temp, state.kumi6);
					else if (i == 7)
						copyTable(temp, state.kumi7);
					else if (i == 8)
						copyTable(temp, state.kumi8);
					else if (i == 9)
						copyTable(temp, state.kumi9);
					else if (i == 10)
						copyTable(temp, state.kumi10);

					rank = state.kumi_info[i][set];
				}
			}
		}
	}
	else
	{ //ロックされていないなら
		for (i = 0; i < 11; i++)
		{
			if (state.kumi_info[i][1] == state.qty && state.kumi_info[i][3] > state.ord && state.kumi_info[i][set] <= rank)
			{

				if (i == 0)
					copyTable(temp, state.kumi0);
				else if (i == 1)
					copyTable(temp, state.kumi1);
				else if (i == 2)
					copyTable(temp, state.kumi2);
				else if (i == 3)
					copyTable(temp, state.kumi3);
				else if (i == 4)
					copyTable(temp, state.kumi4);
				else if (i == 5)
					copyTable(temp, state.kumi5);
				else if (i == 6)
					copyTable(temp, state.kumi6);
				else if (i == 7)
					copyTable(temp, state.kumi7);
				else if (i == 8)
					copyTable(temp, state.kumi8);
				else if (i == 9)
					copyTable(temp, state.kumi9);
				else if (i == 10)
					copyTable(temp, state.kumi10);

				rank = state.kumi_info[i][set];
			}
		}
	}

	copyTable(out_cards, temp);
}

void my_finish_lead(int out_cards[8][15], int my_cards[8][15])
{

	int kuminumber = 0;
	int copy_cards[8][15] = {{0}};
	int flag = 0;
	int saizyaku_kumi = 0;
	int saizyaku_rank = 0;

	clearTable(out_cards);

	state.finish = 1;
	bunkatsu_pre2(my_cards);
	state.finish = 0;

	if (my_cards[4][1] == 2 && my_cards[0][1] == 1)
	{
		copyTable(copy_cards, my_cards);
		copy_cards[4][1] = 0;
		copy_cards[0][1] = 0;
		bunkatsu_pre2(copy_cards);
		if (state.kumi_info[13][0] - state.kumi_info[13][6] <= 1)
		{
			out_cards[0][14] = 2;
			flag = 1;
			printf("s3_fin     %d\n", state.game_count);
		}
	}

	if (state.kumi_info[13][0] - state.kumi_info[13][6] <= 1 && flag == 0)
	{	//「手札の組数」-「流せると判断された組数」<=1
		//outputkumi_info(0);
		//outputkumi_info(1);
		saizyaku_kumi = min_kumi_info(10);
		saizyaku_rank = state.kumi_info[saizyaku_kumi][3];
		/*
		
	   	 kuminumber=min_kumi_info(10);//1枚あたりの流し評価値が最も小さいものを見つける。
		
		if(kuminumber==0)state.kumi_info[0][set]=0;//流し評価値が最も小さいもの、を見つからないようにする。
		else if(kuminumber==1)state.kumi_info[1][set]=0;
		else if(kuminumber==2)state.kumi_info[2][set]=0;
		else if(kuminumber==3)state.kumi_info[3][set]=0;
		else if(kuminumber==4)state.kumi_info[4][set]=0;
		else if(kuminumber==5)state.kumi_info[5][set]=0;
		else if(kuminumber==6)state.kumi_info[6][set]=0;
		else if(kuminumber==7)state.kumi_info[7][set]=0;
		else if(kuminumber==8)state.kumi_info[8][set]=0;
		else if(kuminumber==9)state.kumi_info[9][set]=0;
		else if(kuminumber==10)state.kumi_info[10][set]=0;
		
	*/
		/*
		kuminumber=min_kumi_info_kakou(3);//
		
		if(state.kumi_info[kuminumber][0]==1||state.kumi_info[kuminumber][0]==2){
			
			if(state.kumi_info[kuminumber][3]==6&&state.kumi_info[13][0]>=3){//場に何もないときに8のシングルを出そうとしたら
				state.kumi_info[kuminumber][1]=0;
				
			}
		}
		
		kuminumber=min_kumi_info(15);//既に加工されているため
		if(kuminumber==0)copyTable(out_cards,state.kumi0);
		else if(kuminumber==1)copyTable(out_cards,state.kumi1);
		else if(kuminumber==2)copyTable(out_cards,state.kumi2);
		else if(kuminumber==3)copyTable(out_cards,state.kumi3);
		else if(kuminumber==4)copyTable(out_cards,state.kumi4);
		else if(kuminumber==5)copyTable(out_cards,state.kumi5);
		else if(kuminumber==6)copyTable(out_cards,state.kumi6);
		else if(kuminumber==7)copyTable(out_cards,state.kumi7);
		else if(kuminumber==8)copyTable(out_cards,state.kumi8);
		else if(kuminumber==9)copyTable(out_cards,state.kumi9);
		else if(kuminumber==10)copyTable(out_cards,state.kumi10);
		*/
		if (beEmptyCards(out_cards) == 1)
		{

			for (kuminumber = 0; kuminumber <= 10; kuminumber++)
			{

				if (state.kumi_info[kuminumber][1] <= 4 && state.kumi_info[kuminumber][10] >= STRONG && state.kumi_info[kuminumber][0] == 3)
				{

					if (kuminumber == 0)
						copyTable(out_cards, state.kumi0);
					else if (kuminumber == 1)
						copyTable(out_cards, state.kumi1);
					else if (kuminumber == 2)
						copyTable(out_cards, state.kumi2);
					else if (kuminumber == 3)
						copyTable(out_cards, state.kumi3);
					else if (kuminumber == 4)
						copyTable(out_cards, state.kumi4);
					else if (kuminumber == 5)
						copyTable(out_cards, state.kumi5);
					else if (kuminumber == 6)
						copyTable(out_cards, state.kumi6);
					else if (kuminumber == 7)
						copyTable(out_cards, state.kumi7);
					else if (kuminumber == 8)
						copyTable(out_cards, state.kumi8);
					else if (kuminumber == 9)
						copyTable(out_cards, state.kumi9);
					else if (kuminumber == 10)
						copyTable(out_cards, state.kumi10);
				}
			}
		}

		if (beEmptyCards(out_cards) == 1)
		{

			for (kuminumber = 0; kuminumber <= 10; kuminumber++)
			{

				if (state.kumi_info[kuminumber][1] == 3 && state.kumi_info[kuminumber][10] >= STRONG && state.kumi_info[kuminumber][0] == 2)
				{

					if (kuminumber == 0)
						copyTable(out_cards, state.kumi0);
					else if (kuminumber == 1)
						copyTable(out_cards, state.kumi1);
					else if (kuminumber == 2)
						copyTable(out_cards, state.kumi2);
					else if (kuminumber == 3)
						copyTable(out_cards, state.kumi3);
					else if (kuminumber == 4)
						copyTable(out_cards, state.kumi4);
					else if (kuminumber == 5)
						copyTable(out_cards, state.kumi5);
					else if (kuminumber == 6)
						copyTable(out_cards, state.kumi6);
					else if (kuminumber == 7)
						copyTable(out_cards, state.kumi7);
					else if (kuminumber == 8)
						copyTable(out_cards, state.kumi8);
					else if (kuminumber == 9)
						copyTable(out_cards, state.kumi9);
					else if (kuminumber == 10)
						copyTable(out_cards, state.kumi10);
				}
			}
		}

		if (saizyaku_rank > 6)
		{
			//printf("saiyzaku_rank>6\n");
			if (beEmptyCards(out_cards) == 1)
			{

				for (kuminumber = 0; kuminumber <= 10; kuminumber++)
				{

					if (state.kumi_info[kuminumber][1] == 1 && state.kumi_info[kuminumber][10] >= STRONG)
					{

						if (kuminumber == 0)
							copyTable(out_cards, state.kumi0);
						else if (kuminumber == 1)
							copyTable(out_cards, state.kumi1);
						else if (kuminumber == 2)
							copyTable(out_cards, state.kumi2);
						else if (kuminumber == 3)
							copyTable(out_cards, state.kumi3);
						else if (kuminumber == 4)
							copyTable(out_cards, state.kumi4);
						else if (kuminumber == 5)
							copyTable(out_cards, state.kumi5);
						else if (kuminumber == 6)
							copyTable(out_cards, state.kumi6);
						else if (kuminumber == 7)
							copyTable(out_cards, state.kumi7);
						else if (kuminumber == 8)
							copyTable(out_cards, state.kumi8);
						else if (kuminumber == 9)
							copyTable(out_cards, state.kumi9);
						else if (kuminumber == 10)
							copyTable(out_cards, state.kumi10);
					}
				}
			}

			if (beEmptyCards(out_cards) == 1)
			{

				for (kuminumber = 0; kuminumber <= 10; kuminumber++)
				{

					if (state.kumi_info[kuminumber][1] == 2 && state.kumi_info[kuminumber][10] >= STRONG)
					{

						if (kuminumber == 0)
							copyTable(out_cards, state.kumi0);
						else if (kuminumber == 1)
							copyTable(out_cards, state.kumi1);
						else if (kuminumber == 2)
							copyTable(out_cards, state.kumi2);
						else if (kuminumber == 3)
							copyTable(out_cards, state.kumi3);
						else if (kuminumber == 4)
							copyTable(out_cards, state.kumi4);
						else if (kuminumber == 5)
							copyTable(out_cards, state.kumi5);
						else if (kuminumber == 6)
							copyTable(out_cards, state.kumi6);
						else if (kuminumber == 7)
							copyTable(out_cards, state.kumi7);
						else if (kuminumber == 8)
							copyTable(out_cards, state.kumi8);
						else if (kuminumber == 9)
							copyTable(out_cards, state.kumi9);
						else if (kuminumber == 10)
							copyTable(out_cards, state.kumi10);
					}
				}
			}
		}
		else
		{
			//printf("saiyzaku_rank<=6\n");
			if (beEmptyCards(out_cards) == 1)
			{

				for (kuminumber = 0; kuminumber <= 10; kuminumber++)
				{

					if (state.kumi_info[kuminumber][1] == 1 && state.kumi_info[kuminumber][10] >= STRONG && state.kumi_info[kuminumber][3] != 6)
					{

						if (kuminumber == 0)
							copyTable(out_cards, state.kumi0);
						else if (kuminumber == 1)
							copyTable(out_cards, state.kumi1);
						else if (kuminumber == 2)
							copyTable(out_cards, state.kumi2);
						else if (kuminumber == 3)
							copyTable(out_cards, state.kumi3);
						else if (kuminumber == 4)
							copyTable(out_cards, state.kumi4);
						else if (kuminumber == 5)
							copyTable(out_cards, state.kumi5);
						else if (kuminumber == 6)
							copyTable(out_cards, state.kumi6);
						else if (kuminumber == 7)
							copyTable(out_cards, state.kumi7);
						else if (kuminumber == 8)
							copyTable(out_cards, state.kumi8);
						else if (kuminumber == 9)
							copyTable(out_cards, state.kumi9);
						else if (kuminumber == 10)
							copyTable(out_cards, state.kumi10);
					}
				}
			}

			if (beEmptyCards(out_cards) == 1)
			{

				for (kuminumber = 0; kuminumber <= 10; kuminumber++)
				{

					if (state.kumi_info[kuminumber][1] == 1 && state.kumi_info[kuminumber][10] >= STRONG && state.kumi_info[kuminumber][3] == 6)
					{

						if (kuminumber == 0)
							copyTable(out_cards, state.kumi0);
						else if (kuminumber == 1)
							copyTable(out_cards, state.kumi1);
						else if (kuminumber == 2)
							copyTable(out_cards, state.kumi2);
						else if (kuminumber == 3)
							copyTable(out_cards, state.kumi3);
						else if (kuminumber == 4)
							copyTable(out_cards, state.kumi4);
						else if (kuminumber == 5)
							copyTable(out_cards, state.kumi5);
						else if (kuminumber == 6)
							copyTable(out_cards, state.kumi6);
						else if (kuminumber == 7)
							copyTable(out_cards, state.kumi7);
						else if (kuminumber == 8)
							copyTable(out_cards, state.kumi8);
						else if (kuminumber == 9)
							copyTable(out_cards, state.kumi9);
						else if (kuminumber == 10)
							copyTable(out_cards, state.kumi10);
					}
				}
			}

			if (beEmptyCards(out_cards) == 1)
			{

				for (kuminumber = 0; kuminumber <= 10; kuminumber++)
				{

					if (state.kumi_info[kuminumber][1] == 2 && state.kumi_info[kuminumber][10] >= STRONG && state.kumi_info[kuminumber][3] != 6)
					{

						if (kuminumber == 0)
							copyTable(out_cards, state.kumi0);
						else if (kuminumber == 1)
							copyTable(out_cards, state.kumi1);
						else if (kuminumber == 2)
							copyTable(out_cards, state.kumi2);
						else if (kuminumber == 3)
							copyTable(out_cards, state.kumi3);
						else if (kuminumber == 4)
							copyTable(out_cards, state.kumi4);
						else if (kuminumber == 5)
							copyTable(out_cards, state.kumi5);
						else if (kuminumber == 6)
							copyTable(out_cards, state.kumi6);
						else if (kuminumber == 7)
							copyTable(out_cards, state.kumi7);
						else if (kuminumber == 8)
							copyTable(out_cards, state.kumi8);
						else if (kuminumber == 9)
							copyTable(out_cards, state.kumi9);
						else if (kuminumber == 10)
							copyTable(out_cards, state.kumi10);
					}
				}
			}

			if (beEmptyCards(out_cards) == 1)
			{

				for (kuminumber = 0; kuminumber <= 10; kuminumber++)
				{

					if (state.kumi_info[kuminumber][1] == 2 && state.kumi_info[kuminumber][10] >= STRONG && state.kumi_info[kuminumber][3] == 6)
					{

						if (kuminumber == 0)
							copyTable(out_cards, state.kumi0);
						else if (kuminumber == 1)
							copyTable(out_cards, state.kumi1);
						else if (kuminumber == 2)
							copyTable(out_cards, state.kumi2);
						else if (kuminumber == 3)
							copyTable(out_cards, state.kumi3);
						else if (kuminumber == 4)
							copyTable(out_cards, state.kumi4);
						else if (kuminumber == 5)
							copyTable(out_cards, state.kumi5);
						else if (kuminumber == 6)
							copyTable(out_cards, state.kumi6);
						else if (kuminumber == 7)
							copyTable(out_cards, state.kumi7);
						else if (kuminumber == 8)
							copyTable(out_cards, state.kumi8);
						else if (kuminumber == 9)
							copyTable(out_cards, state.kumi9);
						else if (kuminumber == 10)
							copyTable(out_cards, state.kumi10);
					}
				}
			}
		}

		if (beEmptyCards(out_cards) == 1)
		{

			for (kuminumber = 0; kuminumber <= 10; kuminumber++)
			{

				if (state.kumi_info[kuminumber][1] >= 4 && state.kumi_info[kuminumber][10] >= STRONG)
				{

					if (kuminumber == 0)
						copyTable(out_cards, state.kumi0);
					else if (kuminumber == 1)
						copyTable(out_cards, state.kumi1);
					else if (kuminumber == 2)
						copyTable(out_cards, state.kumi2);
					else if (kuminumber == 3)
						copyTable(out_cards, state.kumi3);
					else if (kuminumber == 4)
						copyTable(out_cards, state.kumi4);
					else if (kuminumber == 5)
						copyTable(out_cards, state.kumi5);
					else if (kuminumber == 6)
						copyTable(out_cards, state.kumi6);
					else if (kuminumber == 7)
						copyTable(out_cards, state.kumi7);
					else if (kuminumber == 8)
						copyTable(out_cards, state.kumi8);
					else if (kuminumber == 9)
						copyTable(out_cards, state.kumi9);
					else if (kuminumber == 10)
						copyTable(out_cards, state.kumi10);
				}
			}
		}

		/*
		if(state.kumi_info[13][0]-state.kumi_info[13][6]==1){
			kuminumber=min_kumi_info(10);//1枚あたりの流し評価値が最も小さいものを見つける。
			
			if(kuminumber==0)state.kumi_info[0][set]=0;//流し評価値が最も小さいもの、を見つからないようにする。
			else if(kuminumber==1)state.kumi_info[1][set]=0;
			else if(kuminumber==2)state.kumi_info[2][set]=0;
			else if(kuminumber==3)state.kumi_info[3][set]=0;
			else if(kuminumber==4)state.kumi_info[4][set]=0;
			else if(kuminumber==5)state.kumi_info[5][set]=0;
			else if(kuminumber==6)state.kumi_info[6][set]=0;
			else if(kuminumber==7)state.kumi_info[7][set]=0;
			else if(kuminumber==8)state.kumi_info[8][set]=0;
			else if(kuminumber==9)state.kumi_info[9][set]=0;
			else if(kuminumber==10)state.kumi_info[10][set]=0;
		}
		
		kuminumber=min_kumi_info(3);
		if(kuminumber==0)copyTable(out_cards,state.kumi0);
		else if(kuminumber==1)copyTable(out_cards,state.kumi1);
		else if(kuminumber==2)copyTable(out_cards,state.kumi2);
		else if(kuminumber==3)copyTable(out_cards,state.kumi3);
		else if(kuminumber==4)copyTable(out_cards,state.kumi4);
		else if(kuminumber==5)copyTable(out_cards,state.kumi5);
		else if(kuminumber==6)copyTable(out_cards,state.kumi6);
		else if(kuminumber==7)copyTable(out_cards,state.kumi7);
		else if(kuminumber==8)copyTable(out_cards,state.kumi8);
		else if(kuminumber==9)copyTable(out_cards,state.kumi9);
		else if(kuminumber==10)copyTable(out_cards,state.kumi10);
		*/

		if (beEmptyCards(out_cards) == 1 && state.kumi_info[13][0] == 1)
		{
			copyTable(out_cards, state.kumi0);
		}
	}
}

void my_finish_follow2(int out_cards[8][15], int my_cards[8][15])
{

	clearTable(out_cards);

	if (state.qty == 1)
	{
		my_finish_followSolo2(out_cards, my_cards, state.joker); //一枚のとき
	}
	else
	{
		if (state.sequence == 0)
		{
			my_finish_followGroup2(out_cards, my_cards, state.joker); //枚数組のとき
		}
		else
		{
			my_finish_followSequence2(out_cards, my_cards, state.joker); //階段のとき
		}
	}
}

void my_finish_followSolo2(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};
	int copy_cards[8][15] = {{0}};
	int copy_cards2[8][15] = {{0}};

	int tsuyosa = 0, ba_suit = 0, kumi_suit = 0;
	int kumi_rank = 0;
	int flag = 0;

	int temp3[8][15] = {{0}};
	int temp4[8][15] = {{0}};
	int temp5[8][15] = {{0}};
	int temp6[8][15] = {{0}};
	int temp7[8][15] = {{0}};

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);
	copyCards(copy_cards, my_cards);

	if (state.suit[0] == 1)
		ba_suit = 0;
	if (state.suit[1] == 1)
		ba_suit = 1;
	if (state.suit[2] == 1)
		ba_suit = 2;
	if (state.suit[3] == 1)
		ba_suit = 3;

	highCards(temp, copy_cards, state.ord);

	if (state.lock == 1)
	{
		lockCards(temp, state.suit);
	}

	//縛りも考慮して、流せる組をすべて書き出す方法をとることに。
	copyCards(temp3, temp);
	copyCards(temp4, my_cards);

	while (qtyOfCards(temp3) > 0 && flag == 0)
	{
		copyCards(copy_cards, my_cards);

		if (state.find_s3 == 0)
			joker_delete(temp3);

		lowSolo2(temp2, temp3, state.joker);

		kumi_rank = get_rank_min(temp2);

		kumi_suit = get_suit(temp2);
		if (kumi_suit == ba_suit)
		{
			tsuyosa = setvalue_single_shibari(temp2, kumi_suit);
		}
		else
		{
			tsuyosa = setvalue_single2(temp2);
		}

		cardsDiff3(temp3, temp2);
		if (tsuyosa >= STRONG)
		{
			cardsDiff3(temp4, temp2); //
		}
	}

	while (qtyOfCards(temp) > 0 && flag == 0)
	{
		copyCards(copy_cards, my_cards);

		if (state.find_s3 == 0)
			joker_delete(temp);

		lowSolo2(temp2, temp, state.joker);

		kumi_rank = get_rank_min(temp2);

		kumi_suit = get_suit(temp2);
		if (kumi_suit == ba_suit)
		{
			tsuyosa = setvalue_single_shibari(temp2, kumi_suit);
		}
		else
		{
			tsuyosa = setvalue_single2(temp2);
		}

		cardsDiff3(temp, temp2);
		cardsDiff3(copy_cards, temp2);

		state.finish = 1;
		bunkatsu_pre2(copy_cards);
		state.finish = 0;
		/*
		if(state.game_count==4406){
			printf("my_cards\n");
			outputCards(my_cards);
			printf("temp\n");
			outputCards(temp);
			printf("temp2\n");
			outputCards(temp2);
		}
		*/
		/*
		printf("my_cards\n");
		outputCards(my_cards);
		printf("copy_cards\n");
		outputCards(copy_cards);
		printf("temp\n");
		outputCards(temp);
		printf("temp2\n");
		outputCards(temp2);
		printf("temp3\n");
		outputCards(temp3);
		printf("temp4\n");
		outputCards(temp4);
		*/

		if (tsuyosa >= STRONG && my_lead5_4(temp5, copy_cards) == 1)
		{
			copyTable(temp6, temp2);
			flag = 2;
			/*
			printf("state.count   %d\n");
			state.count=state.count+1;
			printf("state.count   %d\n");
			*/
		}

		//printf("my_lead5_4 pass\n");

		if (temp2[0][14] == 2 && copy_cards[0][1] == 1)
		{

			if (state.kumi_info[13][0] - state.kumi_info[13][6] <= 1)
			{
			}
			else
			{
				copy_cards[0][1] = 0;

				state.cause_rev = 1;
				bunkatsu_pre2(copy_cards);
				state.cause_rev = 0;

				if (tsuyosa >= STRONG && my_lead5_4(temp5, copy_cards) == 1)
				{
					copyTable(out_cards, temp2);
					flag = 1;
					/*
					state.count=state.count+1;
					printf("%d\n",state.game_count);
					printf("kakumeisitekatu   %d   %d\n",state.count,state.game_count);
					outputCards(temp2);
					outputCards(copy_cards);
					*/
				}
			}
		}

		//printf("[0][14] [0][1] pass\n");

		if (tsuyosa >= STRONG && state.kumi_info[13][0] - state.kumi_info[13][6] <= 1)
		{
			copyTable(out_cards, temp2);
			flag = 1;
		}
		if (tsuyosa >= STRONG - 1 && state.kumi_info[13][0] - state.kumi_info[13][6] <= 1 && state.submit_s3 == 0 && state.have_s3 == 0 && state.player_number > 4 && state.rest_cards[4][1] != 0 && flag == 0)
		{
			copyTable(temp7, temp2);
			flag = 3;
		}

		//printf("[13][0] [13][6] pass\n");

		if (kumi_suit == ba_suit && flag == 0 && kumi_rank != 6)
		{
			bunkatsu_pre2(temp4);
			if (tsuyosa >= STRONG && state.kumi_info[13][0] - state.kumi_info[13][6] <= 1)
			{
				copyTable(out_cards, temp2);
				flag = 1;
				printf("yoshi_3\n");
			}
		}

		//printf("[13][0] [13][6] pass2\n");
	}
	if (flag == 2 && beEmptyCards(out_cards) == 1)
	{
		copyTable(out_cards, temp6);
	}
	if (flag == 3 && beEmptyCards(out_cards) == 1)
	{
		copyTable(out_cards, temp7);
		state.count3 = state.count3 + 1;
		printf("2   99   %d   %d\n", state.count3, state.game_count);
	}
}

void my_finish_followGroup2(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int group[8][15];
	int ngroup[8][15];

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};
	int copy_cards[8][15] = {{0}};

	int tsuyosa = 0, ba_suitsum = 0, kumi_suitsum = 0, j = 0;

	int rank2 = 0, pattern = 0;
	int flag = 0;
	int tsuyosa2 = 0;

	int temp5[8][15] = {{0}};
	int temp6[8][15] = {{0}};
	int temp7[8][15] = {{0}};

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);

	copyTable(copy_cards, my_cards);

	if (state.suit[0] == 1)
		ba_suitsum = ba_suitsum + 1;
	if (state.suit[1] == 1)
		ba_suitsum = ba_suitsum + 2;
	if (state.suit[2] == 1)
		ba_suitsum = ba_suitsum + 4;
	if (state.suit[3] == 1)
		ba_suitsum = ba_suitsum + 8;

	highCards(temp, copy_cards, state.ord); //場より強いカードを残す
	if (state.lock == 1)
	{								 //ロックされているとき
		lockCards(temp, state.suit); //出せないカードを除去
	}
	makeJGroupTable2(group, temp); //残ったものから枚数組を書き出す
	nCards2(ngroup, group, state.qty);
	/*
	printf("state.suit[0]     %d\n",state.suit[0]);
	printf("state.suit[1]     %d\n",state.suit[1]);
	printf("state.suit[2]     %d\n",state.suit[2]);
	printf("state.suit[3]     %d\n",state.suit[3]);
	*/
	/*
	printf("temp\n");
	outputCards(temp);
	printf("group\n");
	outputCards(group);
	printf("ngroup\n");
	outputCards(ngroup);
	*/

	if (qtyOfCards(ngroup) >= state.qty)
	{

		for (rank2 = state.ord + 1; rank2 < 14; rank2++)
		{
			for (pattern = 0; pattern < 6; pattern++)
			{

				copyTable(copy_cards, my_cards);

				Group2(temp2, copy_cards, ngroup, rank2, state.qty, pattern);

				if (qtyOfCards(temp2) == state.qty)
				{

					//outputCards(temp2);

					kumi_suitsum = get_suitsum(temp2);
					if (kumi_suitsum == ba_suitsum)
					{
						tsuyosa = setvalue_pair_shibari(temp2, kumi_suitsum);
						tsuyosa2 = setvalue_pair_shibari3(temp2, kumi_suitsum);
					}
					else
					{
						tsuyosa = setvalue_pair2(temp2);
						tsuyosa2 = setvalue_pair3(temp2);
					}

					cardsDiff3(copy_cards, temp2);
					state.finish = 1;
					bunkatsu_pre2(copy_cards);
					state.finish = 0;

					//printf("tsuyosa %d\n",tsuyosa);
					//printf("state.kumi_info[13][0] %d\n",state.kumi_info[13][0]);
					//printf("state.kumi_info[13][6] %d\n",state.kumi_info[13][6]);
					/*
					printf("tsuyosa   %d\n",tsuyosa);
					printf("temp2\n");
					outputCards(temp2);
					*/

					if (tsuyosa >= STRONG && my_lead5_4(temp5, copy_cards) == 1)
					{
						copyTable(temp6, temp2);
						flag = 2;
						/*
						state.count=state.count+1;
						printf("%d\n",state.game_count);
						printf("kakumeisitekatu   %d   %d\n",state.count,state.game_count);
						outputCards(temp2);
						outputCards(copy_cards);
						*/
					}

					if (tsuyosa >= STRONG && state.kumi_info[13][0] - state.kumi_info[13][6] <= 1)
					{
						copyTable(out_cards, temp2);
						flag = 1;
					}
					if (tsuyosa2 >= (STRONG - 1) * state.qty + 1 && state.kumi_info[13][0] - state.kumi_info[13][6] <= 1 && state.player_number > 3 && state.rest_cards[4][1] == 0 && state.kumi_info[13][6] > 2 && flag == 0)
					{
						copyTable(temp7, temp2);
						flag = 3;
					}
				}
			}
			if (flag == 1)
				break;
		}
	}

	if (flag == 2 && beEmptyCards(out_cards) == 1)
	{
		copyTable(out_cards, temp6);
	}

	if (flag == 3 && beEmptyCards(out_cards) == 1)
	{
		copyTable(out_cards, temp7);
		state.count2 = state.count2 + 1;
		printf("99   %d   %d\n", state.count2, state.game_count);
	}
}

void my_finish_followSequence2(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};

	int copy_cards[8][15] = {{0}};

	int tsuyosa = 0, ba_suit = 0, kumi_suit = 0, j = 0;
	int mark = 0, pattern = 0;

	int tsuyosa2 = 0;

	int flag = 0;
	int temp5[8][15] = {{0}};
	int temp6[8][15] = {{0}};
	int temp7[8][15] = {{0}};

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);

	copyTable(copy_cards, my_cards);

	if (state.suit[0] == 1)
		ba_suit = 0;
	if (state.suit[1] == 1)
		ba_suit = 1;
	if (state.suit[2] == 1)
		ba_suit = 2;
	if (state.suit[3] == 1)
		ba_suit = 3;

	highCards(temp, my_cards, state.ord); //場より強いカードを残す
	if (state.lock == 1)
	{								 //ロックされているとき
		lockCards(temp, state.suit); //出せないカードを除去
	}

	for (pattern = 0; pattern < 12; pattern++)
	{
		for (mark = 0; mark < 4; mark++)
		{

			copyCards(copy_cards, my_cards);

			Sequence2(temp2, temp, mark, state.qty, pattern);

			if (qtyOfCards(temp2) == state.qty && get_rank_min(temp2) > state.ord)
			{
				kumi_suit = get_suit(temp2);
				if (kumi_suit == ba_suit)
				{
					tsuyosa = setvalue_kaidan_shibari(temp2, kumi_suit);
					tsuyosa2 = setvalue_kaidan_shibari3(temp2, kumi_suit);
				}
				else
				{
					tsuyosa = setvalue_kaidan2(temp2);
					tsuyosa2 = setvalue_kaidan3(temp2);
				}

				cardsDiff3(copy_cards, temp2);
				state.finish = 1;
				bunkatsu_pre2(copy_cards);
				state.finish = 0;

				if (tsuyosa >= STRONG && my_lead5_4(temp5, copy_cards) == 1)
				{
					copyTable(temp6, temp2);
					flag = 2;
					/*
					state.count=state.count+1;
					printf("%d\n",state.game_count);
					printf("kakumeisitekatu   %d   %d\n",state.count,state.game_count);
					outputCards(temp2);
					outputCards(copy_cards);
					*/
				}

				if (tsuyosa >= STRONG && state.kumi_info[13][0] - state.kumi_info[13][6] <= 1)
				{
					copyTable(out_cards, temp2);
					flag = 1;
				}

				if (tsuyosa2 >= (STRONG - 1) * state.qty + 1 && state.kumi_info[13][0] - state.kumi_info[13][6] <= 1 && state.player_number > 3 && state.rest_cards[4][1] == 0 && state.kumi_info[13][6] > 2 && flag == 0)
				{
					copyTable(temp7, temp2);
					flag = 3;
				}
			}
		}
		if (flag == 1)
			break;
	}

	if (flag == 2 && beEmptyCards(out_cards) == 1)
	{
		copyTable(out_cards, temp6);
	}

	if (flag == 3 && beEmptyCards(out_cards) == 1)
	{
		copyTable(out_cards, temp7);
		state.count2 = state.count2 + 1;
		printf("99   %d   %d\n", state.count2, state.game_count);
	}
}
/*
void my_finish_followSequence2(int out_cards[8][15],int my_cards[8][15],int joker_flag){

  int seq[8][15];
  int nseq[8][15];
 
 	int temp[8][15]={{0}}; 
	int temp2[8][15]={{0}};
	int copy_cards[8][15]={{0}};
	int copy_cards2[8][15]={{0}};
	int copy_cards3[8][15]={{0}};
	int copy_cards4[8][15]={{0}};
	
	int tsuyosa=0,ba_suit=0,kumi_suit=0,j=0;
	
  clearTable(out_cards);
  bunkatsu_pre2(my_cards);

  copyTable(copy_cards,my_cards);
  copyTable(copy_cards2,my_cards);
  copyTable(copy_cards3,my_cards);
  copyTable(copy_cards4,my_cards);
	
  if(state.suit[0]==1)ba_suit=0;
  if(state.suit[1]==1)ba_suit=1;
  if(state.suit[2]==1)ba_suit=2;
  if(state.suit[3]==1)ba_suit=3;
	
  highCards(temp,my_cards,state.ord);          //場より強いカードを残す
  if(state.lock==1){                           //ロックされているとき
    lockCards(temp,state.suit);                //出せないカードを除去
  }
  makeJKaidanTable(seq,temp);                   //階段を書き出す
  nCards2(nseq,seq,state.qty);
 
	
  	 lowSequence(temp2,copy_cards,nseq);//弱いほうから1組
	 if(qtyOfCards(temp2)==state.qty){
	 	kumi_suit=get_suit(temp2);
		if(kumi_suit==ba_suit){
			tsuyosa=setvalue_kaidan_shibari(temp2,kumi_suit);
		}else{
			tsuyosa=setvalue_kaidan2(temp2);
		}
	 	
		cardsDiff3(copy_cards,temp2);
	 	bunkatsu_pre2(copy_cards);
		
	 	if(tsuyosa>=STRONG&&state.kumi_info[13][0]-state.kumi_info[13][6]<=1){
	 		copyTable(out_cards,temp2);
	 	}
	 }
	
	if(qtyOfCards(out_cards)==0){
		clearTable(temp2);
		highSequence(temp2,copy_cards2,nseq);//強いほうから1組
		if(qtyOfCards(temp2)==state.qty){
		 	kumi_suit=get_suit(temp2);
			if(kumi_suit==ba_suit){
				tsuyosa=setvalue_kaidan_shibari(temp2,kumi_suit);
			}else{
				tsuyosa=setvalue_kaidan2(temp2);
			}
		 	
			cardsDiff3(copy_cards2,temp2);
		 	bunkatsu_pre2(copy_cards2);
			
		 	if(tsuyosa>=STRONG&&state.kumi_info[13][0]-state.kumi_info[13][6]<=1){
		 		copyTable(out_cards,temp2);
		 	}
		}
	 }
	
	
	if(state.joker==1){
    //場と同じ枚数の階段が無いときジョーカーを使って探す
   	 	makeJKaidanTable(seq,temp);
   		nCards2(nseq,seq,state.qty);   
		
		if(qtyOfCards(out_cards)==0){	
			clearTable(temp2);
		    lowSequence(temp2,copy_cards3,nseq);//弱いほうから1組
			if(qtyOfCards(temp2)==state.qty){
			 	kumi_suit=get_suit(temp2);
				if(kumi_suit==ba_suit){
					tsuyosa=setvalue_kaidan_shibari(temp2,kumi_suit);
				}else{
					tsuyosa=setvalue_kaidan2(temp2);
				}
			 	
				cardsDiff3(copy_cards3,temp2);
			 	bunkatsu_pre2(copy_cards3);
				
			 	if(tsuyosa>=STRONG&&state.kumi_info[13][0]-state.kumi_info[13][6]<=1){
			 		copyTable(out_cards,temp2);
			 	}
		 	}
		}
		
		if(qtyOfCards(out_cards)==0){
			clearTable(temp2);
			highSequence(temp2,copy_cards4,nseq);//強いほうから1組
			if(qtyOfCards(temp2)==state.qty){
			 	kumi_suit=get_suit(temp2);
				if(kumi_suit==ba_suit){
					tsuyosa=setvalue_kaidan_shibari(temp2,kumi_suit);
				}else{
					tsuyosa=setvalue_kaidan2(temp2);
				}
			 	
				cardsDiff3(copy_cards4,temp2);
			 	bunkatsu_pre2(copy_cards4);
				
			 	if(tsuyosa>=STRONG&&state.kumi_info[13][0]-state.kumi_info[13][6]<=1){
			 		copyTable(out_cards,temp2);
			 	}
			}
		 }
 
 	 }
	
}
*/
void my_finish_follow2_o(int out_cards[8][15], int my_cards[8][15])
{

	int rest_joker = 0;

	clearTable(out_cards);

	if (my_cards[4][1] == 2)
		rest_joker = 1;

	if (state.qty2 == 1)
	{
		my_finish_followSolo2_o(out_cards, my_cards, rest_joker); //一枚のとき
	}
	else
	{
		if (state.sequence2 == 0)
		{
			my_finish_followGroup2_o(out_cards, my_cards, rest_joker); //枚数組のとき
		}
		else
		{
			my_finish_followSequence2_o(out_cards, my_cards, rest_joker); //階段のとき
		}
	}
}

void my_finish_followSolo2_o(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};
	int copy_cards[8][15] = {{0}};

	int tsuyosa = 0, ba_suit = 0, kumi_suit = 0;
	int kumi_rank = 0;
	int flag = 0;

	int temp3[8][15] = {{0}};
	int temp4[8][15] = {{0}};

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);
	copyCards(copy_cards, my_cards);

	if (state.suit2[0] == 1)
		ba_suit = 0;
	if (state.suit2[1] == 1)
		ba_suit = 1;
	if (state.suit2[2] == 1)
		ba_suit = 2;
	if (state.suit2[3] == 1)
		ba_suit = 3;

	highCards(temp, copy_cards, state.ord2);

	if (state.lock == 1)
	{
		lockCards(temp, state.suit2);
	}

	//縛りも考慮して、流せる組をすべて書き出す方法をとることに。
	copyCards(temp3, temp);
	copyCards(temp4, my_cards);
	while (qtyOfCards(temp3) > 0 && flag == 0)
	{
		copyCards(copy_cards, my_cards);

		if (state.find_s3 == 0)
			joker_delete(temp3);

		lowSolo2(temp2, temp3, joker_flag);
		/*
	printf("temp3\n");
	outputCards(temp3);
	printf("temp2\n");
	outputCards(temp2);
	*/

		kumi_rank = get_rank_min(temp2);

		kumi_suit = get_suit(temp2);
		if (kumi_suit == ba_suit)
		{
			tsuyosa = setvalue_single_shibari(temp2, kumi_suit);
		}
		else
		{
			tsuyosa = setvalue_single2(temp2);
		}

		cardsDiff3(temp3, temp2);
		if (tsuyosa >= STRONG)
		{
			cardsDiff3(temp4, temp2); //
		}
	}

	while (qtyOfCards(temp) > 0 && flag == 0)
	{
		copyCards(copy_cards, my_cards);

		if (state.find_s3 == 0)
			joker_delete(temp);

		lowSolo2(temp2, temp, joker_flag);
		/*
		printf("temp3\n");
		outputCards(temp3);
		*/

		kumi_rank = get_rank_min(temp2);

		kumi_suit = get_suit(temp2);
		if (kumi_suit == ba_suit)
		{
			tsuyosa = setvalue_single_shibari(temp2, kumi_suit);
		}
		else
		{
			tsuyosa = setvalue_single2(temp2);
		}

		cardsDiff3(temp, temp2);
		cardsDiff3(copy_cards, temp2);

		state.finish = 1;
		bunkatsu_pre2(copy_cards);
		state.finish = 0;

		if (temp2[0][14] == 2 && copy_cards[0][1] == 1)
		{

			if (state.kumi_info[13][0] - state.kumi_info[13][6] <= 1)
			{
			}
			else
			{
				copy_cards[0][1] = 0;

				state.cause_rev = 1;
				bunkatsu_pre2(copy_cards);
				state.cause_rev = 0;
			}
		}

		if (tsuyosa >= STRONG && state.kumi_info[13][0] - state.kumi_info[13][6] <= 1)
		{
			copyTable(out_cards, temp2);
			flag = 1;
		}

		if (kumi_suit == ba_suit && flag == 0 && kumi_rank != 6)
		{
			bunkatsu_pre2(temp4);
			if (tsuyosa >= STRONG && state.kumi_info[13][0] - state.kumi_info[13][6] <= 1)
			{
				copyTable(out_cards, temp2);
				flag = 1;
				printf("yoshi_3\n");
			}
		}
		/*
		printf("my_cards\n");
		outputCards(my_cards);
		printf("temp\n");
		outputCards(temp);
		printf("temp2\n");
		outputCards(temp2);
		
		printf("tsuyosa   %d\n",tsuyosa);
		printf("state.kumi_info[13][0]   %d\n",state.kumi_info[13][0]);
		printf("state.kumi_info[13][6]   %d\n",state.kumi_info[13][6]);
		
		printf("kumi_suit   %d\n",kumi_suit);
		printf("ba_suit   %d\n",ba_suit);
		printf("state.ord2   %d\n",state.ord2);
		printf("state.qty2   %d\n",state.qty2);
		*/
	}
}

void my_finish_followGroup2_o(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int group[8][15];
	int ngroup[8][15];

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};
	int copy_cards[8][15] = {{0}};

	int tsuyosa = 0, ba_suitsum = 0, kumi_suitsum = 0, j = 0;

	int rank2 = 0, pattern = 0;
	int flag = 0;

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);

	copyTable(copy_cards, my_cards);

	if (state.suit2[0] == 1)
		ba_suitsum = ba_suitsum + 1;
	if (state.suit2[1] == 1)
		ba_suitsum = ba_suitsum + 2;
	if (state.suit2[2] == 1)
		ba_suitsum = ba_suitsum + 4;
	if (state.suit2[3] == 1)
		ba_suitsum = ba_suitsum + 8;

	highCards(temp, copy_cards, state.ord2); //場より強いカードを残す
	if (state.lock == 1)
	{								  //ロックされているとき
		lockCards(temp, state.suit2); //出せないカードを除去
	}
	makeJGroupTable2(group, temp); //残ったものから枚数組を書き出す
	nCards2(ngroup, group, state.qty2);
	/*
	printf("state.suit[0]     %d\n",state.suit[0]);
	printf("state.suit[1]     %d\n",state.suit[1]);
	printf("state.suit[2]     %d\n",state.suit[2]);
	printf("state.suit[3]     %d\n",state.suit[3]);
	*/
	/*
	printf("temp\n");
	outputCards(temp);
	printf("group\n");
	outputCards(group);
	printf("ngroup\n");
	outputCards(ngroup);
	*/

	if (qtyOfCards(ngroup) >= state.qty2)
	{

		for (rank2 = state.ord2 + 1; rank2 < 14; rank2++)
		{
			for (pattern = 0; pattern < 6; pattern++)
			{

				copyTable(copy_cards, my_cards);

				Group2(temp2, copy_cards, ngroup, rank2, state.qty2, pattern);

				if (qtyOfCards(temp2) == state.qty2)
				{

					//outputCards(temp2);

					kumi_suitsum = get_suitsum(temp2);
					if (kumi_suitsum == ba_suitsum)
					{
						tsuyosa = setvalue_pair_shibari(temp2, kumi_suitsum);
					}
					else
					{
						tsuyosa = setvalue_pair2(temp2);
					}

					cardsDiff3(copy_cards, temp2);
					state.finish = 1;
					bunkatsu_pre2(copy_cards);
					state.finish = 0;

					//printf("tsuyosa %d\n",tsuyosa);
					//printf("state.kumi_info[13][0] %d\n",state.kumi_info[13][0]);
					//printf("state.kumi_info[13][6] %d\n",state.kumi_info[13][6]);
					/*
					printf("tsuyosa   %d\n",tsuyosa);
					printf("temp2\n");
					outputCards(temp2);
					*/

					if (tsuyosa >= STRONG && state.kumi_info[13][0] - state.kumi_info[13][6] <= 1)
					{
						copyTable(out_cards, temp2);
						flag = 1;
					}
				}
			}
			if (flag == 1)
				break;
		}
	}
}

void my_finish_followSequence2_o(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};

	int copy_cards[8][15] = {{0}};

	int tsuyosa = 0, ba_suit = 0, kumi_suit = 0, j = 0;
	int mark = 0, pattern = 0;

	int flag = 0;

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);

	copyTable(copy_cards, my_cards);

	if (state.suit2[0] == 1)
		ba_suit = 0;
	if (state.suit2[1] == 1)
		ba_suit = 1;
	if (state.suit2[2] == 1)
		ba_suit = 2;
	if (state.suit2[3] == 1)
		ba_suit = 3;

	highCards(temp, my_cards, state.ord2); //場より強いカードを残す
	if (state.lock == 1)
	{								  //ロックされているとき
		lockCards(temp, state.suit2); //出せないカードを除去
	}

	for (pattern = 0; pattern < 12; pattern++)
	{
		for (mark = 0; mark < 4; mark++)
		{

			copyCards(copy_cards, my_cards);

			Sequence2(temp2, temp, mark, state.qty2, pattern);

			if (qtyOfCards(temp2) == state.qty2 && get_rank_min(temp2) > state.ord2)
			{
				kumi_suit = get_suit(temp2);
				if (kumi_suit == ba_suit)
				{
					tsuyosa = setvalue_kaidan_shibari(temp2, kumi_suit);
				}
				else
				{
					tsuyosa = setvalue_kaidan2(temp2);
				}

				cardsDiff3(copy_cards, temp2);
				state.cause_rev = 1;
				bunkatsu_pre2(copy_cards);
				state.cause_rev = 0;

				if (tsuyosa >= STRONG && state.kumi_info[13][0] - state.kumi_info[13][6] <= 1)
				{
					copyTable(out_cards, temp2);
					flag = 1;
				}
			}
		}
		if (flag == 1)
			break;
	}
}

void my_finish_follow2_o_rev(int out_cards[8][15], int my_cards[8][15])
{

	int rest_joker = 0;

	clearTable(out_cards);

	if (my_cards[4][1] == 2)
		rest_joker = 1;

	if (state.qty2 == 1)
	{
		my_finish_followSolo2_o_rev(out_cards, my_cards, rest_joker); //一枚のとき
	}
	else
	{
		if (state.sequence2 == 0)
		{
			my_finish_followGroup2_o_rev(out_cards, my_cards, rest_joker); //枚数組のとき
		}
		else
		{
			my_finish_followSequence2_o_rev(out_cards, my_cards, rest_joker); //階段のとき
		}
	}
}

void my_finish_followSolo2_o_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};
	int copy_cards[8][15] = {{0}};

	int tsuyosa = 0, ba_suit = 0, kumi_suit = 0;
	int kumi_rank = 0;
	int flag = 0;

	int temp3[8][15] = {{0}};
	int temp4[8][15] = {{0}};

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);
	copyCards(copy_cards, my_cards);

	if (state.suit2[0] == 1)
		ba_suit = 0;
	if (state.suit2[1] == 1)
		ba_suit = 1;
	if (state.suit2[2] == 1)
		ba_suit = 2;
	if (state.suit2[3] == 1)
		ba_suit = 3;

	lowCards(temp, copy_cards, state.ord2);

	if (state.lock == 1)
	{
		lockCards(temp, state.suit2);
	}

	//縛りも考慮して、流せる組をすべて書き出す方法をとることに。
	copyCards(temp3, temp);
	copyCards(temp4, my_cards);
	while (qtyOfCards(temp3) > 0 && flag == 0)
	{
		copyCards(copy_cards, my_cards);

		if (state.find_s3 == 0)
			joker_delete(temp3);

		highSolo2(temp2, temp3, joker_flag);
		/*
	printf("temp3\n");
	outputCards(temp3);
	printf("temp2\n");
	outputCards(temp2);
	*/

		kumi_rank = get_rank_min(temp2);

		kumi_suit = get_suit(temp2);
		if (kumi_suit == ba_suit)
		{
			tsuyosa = setvalue_single_shibari(temp2, kumi_suit);
		}
		else
		{
			tsuyosa = setvalue_single2(temp2);
		}

		cardsDiff3(temp3, temp2);
		if (tsuyosa >= STRONG)
		{
			cardsDiff3(temp4, temp2); //
		}
	}

	while (qtyOfCards(temp) > 0 && flag == 0)
	{
		copyCards(copy_cards, my_cards);

		if (state.find_s3 == 0)
			joker_delete(temp);

		highSolo2(temp2, temp, joker_flag);

		kumi_rank = get_rank_min(temp2);

		kumi_suit = get_suit(temp2);
		if (kumi_suit == ba_suit)
		{
			tsuyosa = setvalue_single_shibari(temp2, kumi_suit);
		}
		else
		{
			tsuyosa = setvalue_single2(temp2);
		}

		cardsDiff3(temp, temp2);
		cardsDiff3(copy_cards, temp2);

		state.finish = 1;
		bunkatsu_pre2(copy_cards);
		state.finish = 0;
		/*
		if(temp2[0][14]==2&&copy_cards[0][1]==1){
			
			if(state.kumi_info[13][0]-state.kumi_info[13][7]<=1){
				
			}else{
				copy_cards[0][1]=0;
				
				state.cause_rev=1;
				bunkatsu_pre2(copy_cards);
				state.cause_rev=0;
			}
		}
		*/

		if (tsuyosa >= STRONG && state.kumi_info[13][0] - state.kumi_info[13][7] <= 1)
		{
			copyTable(out_cards, temp2);
			flag = 1;
		}

		if (kumi_suit == ba_suit && flag == 0 && kumi_rank != 6)
		{
			bunkatsu_pre2(temp4);
			if (tsuyosa >= STRONG && state.kumi_info[13][0] - state.kumi_info[13][7] <= 1)
			{
				copyTable(out_cards, temp2);
				flag = 1;
				//printf("yoshi_3\n");
			}
		}
		/*
		printf("my_cards\n");
		outputCards(my_cards);
		printf("temp\n");
		outputCards(temp);
		printf("temp2\n");
		outputCards(temp2);
		
		printf("tsuyosa   %d\n",tsuyosa);
		printf("state.kumi_info[13][0]   %d\n",state.kumi_info[13][0]);
		printf("state.kumi_info[13][6]   %d\n",state.kumi_info[13][6]);
		
		printf("kumi_suit   %d\n",kumi_suit);
		printf("ba_suit   %d\n",ba_suit);
		printf("state.ord2   %d\n",state.ord2);
		printf("state.qty2   %d\n",state.qty2);
		*/
	}
}

void my_finish_followGroup2_o_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int group[8][15];
	int ngroup[8][15];

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};
	int copy_cards[8][15] = {{0}};

	int tsuyosa = 0, ba_suitsum = 0, kumi_suitsum = 0, j = 0;

	int rank2 = 0, pattern = 0;
	int flag = 0;

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);

	copyTable(copy_cards, my_cards);

	if (state.suit2[0] == 1)
		ba_suitsum = ba_suitsum + 1;
	if (state.suit2[1] == 1)
		ba_suitsum = ba_suitsum + 2;
	if (state.suit2[2] == 1)
		ba_suitsum = ba_suitsum + 4;
	if (state.suit2[3] == 1)
		ba_suitsum = ba_suitsum + 8;

	lowCards(temp, copy_cards, state.ord2); //場より強いカードを残す
	if (state.lock == 1)
	{								  //ロックされているとき
		lockCards(temp, state.suit2); //出せないカードを除去
	}
	makeJGroupTable2(group, temp); //残ったものから枚数組を書き出す
	nCards2(ngroup, group, state.qty2);
	/*
	printf("state.suit[0]     %d\n",state.suit[0]);
	printf("state.suit[1]     %d\n",state.suit[1]);
	printf("state.suit[2]     %d\n",state.suit[2]);
	printf("state.suit[3]     %d\n",state.suit[3]);
	*/
	/*
	printf("temp\n");
	outputCards(temp);
	printf("group\n");
	outputCards(group);
	printf("ngroup\n");
	outputCards(ngroup);
	*/

	if (qtyOfCards(ngroup) >= state.qty2)
	{

		for (rank2 = state.ord2 - 1; rank2 > 0; rank2--)
		{
			for (pattern = 0; pattern < 6; pattern++)
			{

				copyTable(copy_cards, my_cards);

				Group2(temp2, copy_cards, ngroup, rank2, state.qty2, pattern);

				if (qtyOfCards(temp2) == state.qty2)
				{

					//outputCards(temp2);
					kumi_suitsum = get_suitsum(temp2);
					if (kumi_suitsum == ba_suitsum)
					{
						tsuyosa = setvalue_pair_shibari(temp2, kumi_suitsum);
					}
					else
					{
						tsuyosa = setvalue_pair2(temp2);
					}

					cardsDiff3(copy_cards, temp2);
					state.finish = 1;
					bunkatsu_pre2(copy_cards);
					state.finish = 0;

					//printf("tsuyosa %d\n",tsuyosa);
					//printf("state.kumi_info[13][0] %d\n",state.kumi_info[13][0]);
					//printf("state.kumi_info[13][6] %d\n",state.kumi_info[13][6]);
					/*
					printf("tsuyosa   %d\n",tsuyosa);
					printf("temp2\n");
					outputCards(temp2);
					*/

					if (tsuyosa >= STRONG && state.kumi_info[13][0] - state.kumi_info[13][7] <= 1)
					{
						copyTable(out_cards, temp2);
						flag = 1;
					}
				}
			}
			if (flag == 1)
				break;
		}
	}
}

void my_finish_followSequence2_o_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};

	int copy_cards[8][15] = {{0}};

	int tsuyosa = 0, ba_suit = 0, kumi_suit = 0, j = 0;
	int mark = 0, pattern = 0;

	int flag = 0;

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);

	copyTable(copy_cards, my_cards);

	if (state.suit2[0] == 1)
		ba_suit = 0;
	if (state.suit2[1] == 1)
		ba_suit = 1;
	if (state.suit2[2] == 1)
		ba_suit = 2;
	if (state.suit2[3] == 1)
		ba_suit = 3;

	lowCards(temp, my_cards, state.ord2); //場より強いカードを残す
	if (state.lock == 1)
	{								  //ロックされているとき
		lockCards(temp, state.suit2); //出せないカードを除去
	}

	for (pattern = 11; pattern >= 0; pattern++)
	{
		for (mark = 0; mark < 4; mark++)
		{

			copyCards(copy_cards, my_cards);

			Sequence2(temp2, temp, mark, state.qty2, pattern);

			if (qtyOfCards(temp2) == state.qty2 && get_rank_min(temp2) > state.ord2)
			{
				kumi_suit = get_suit(temp2);
				if (kumi_suit == ba_suit)
				{
					tsuyosa = setvalue_kaidan_shibari(temp2, kumi_suit);
				}
				else
				{
					tsuyosa = setvalue_kaidan2(temp2);
				}

				cardsDiff3(copy_cards, temp2);
				state.finish = 1;
				bunkatsu_pre2(copy_cards);
				state.finish = 0;

				if (tsuyosa >= STRONG && state.kumi_info[13][0] - state.kumi_info[13][7] <= 1)
				{
					copyTable(out_cards, temp2);
					flag = 1;
				}
			}
		}
		if (flag == 1)
			break;
	}
}

void my_finish_follow3(int out_cards[8][15], int my_cards[8][15])
{

	clearTable(out_cards);

	if (state.qty == 1)
	{
		my_finish_followSolo3(out_cards, my_cards, state.joker); //一枚のとき
	}
	else
	{
		if (state.sequence == 0)
		{
			my_finish_followGroup3(out_cards, my_cards, state.joker); //枚数組のとき
		}
		else
		{
			my_finish_followSequence3(out_cards, my_cards, state.joker); //階段のとき
		}
	}
}

void my_finish_followSolo3(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};
	int copy_cards[8][15] = {{0}};

	int tsuyosa = 0, ba_suit = 0, kumi_suit = 0;
	int flag = 0;

	int kumi_rank = 0;

	int temp3[8][15] = {{0}};
	int temp4[8][15] = {{0}};

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);
	copyCards(copy_cards, my_cards);

	if (state.suit[0] == 1)
		ba_suit = 0;
	if (state.suit[1] == 1)
		ba_suit = 1;
	if (state.suit[2] == 1)
		ba_suit = 2;
	if (state.suit[3] == 1)
		ba_suit = 3;

	highCards(temp, copy_cards, state.ord);

	if (state.lock == 1)
	{
		lockCards(temp, state.suit);
	}

	copyCards(temp3, temp);
	copyCards(temp4, my_cards);
	while (qtyOfCards(temp3) > 0 && flag == 0)
	{
		copyCards(copy_cards, my_cards);

		if (state.find_s3 == 0)
			joker_delete(temp3);

		lowSolo2(temp2, temp3, state.joker);

		kumi_rank = get_rank_min(temp2);

		kumi_suit = get_suit(temp2);
		if (kumi_suit == ba_suit)
		{
			tsuyosa = setvalue_single_shibari(temp2, kumi_suit);
		}
		else
		{
			tsuyosa = setvalue_single2(temp2);
		}

		cardsDiff3(temp3, temp2);
		if (tsuyosa >= STRONG)
		{
			cardsDiff3(temp4, temp2); //
		}
	}

	while (qtyOfCards(temp) > 0 && flag == 0)
	{
		copyCards(copy_cards, my_cards);

		lowSolo(temp2, temp, state.joker);

		kumi_rank = get_rank_min(temp2);

		kumi_suit = get_suit(temp2);
		if (kumi_suit == ba_suit)
		{
			tsuyosa = setvalue_single_shibari(temp2, kumi_suit);
		}
		else
		{
			tsuyosa = setvalue_single2(temp2);
		}

		cardsDiff3(temp, temp2);

		cardsDiff3(copy_cards, temp2);

		if (temp2[0][14] == 2 && copy_cards[0][1] == 1)
		{
			copy_cards[0][1] = 0;
			//printf("test//     %d\n",state.game_count);
		}

		bunkatsu_pre2(copy_cards);

		if (state.kumi_info[13][0] - state.kumi_info[13][6] <= 1 && qtyOfCards5(temp2, 6) == 0)
		{
			copyTable(out_cards, temp2);
			flag = 1;
		}

		if (kumi_suit == ba_suit && flag == 0 && kumi_rank != 6)
		{
			bunkatsu_pre2(temp4);
			if (tsuyosa >= STRONG && state.kumi_info[13][0] - state.kumi_info[13][6] <= 1)
			{
				copyTable(out_cards, temp2);
				flag = 1;
				//printf("yoshi_3\n");
			}
		}

		/*
		printf("my_cards\n");
		outputCards(my_cards);
		printf("temp\n");
		outputCards(temp);
		printf("temp2\n");
		outputCards(temp2);
		*/
	}
}

void my_finish_followGroup3(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int group[8][15];
	int ngroup[8][15];

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};
	int copy_cards[8][15] = {{0}};

	int tsuyosa = 0, ba_suitsum = 0, kumi_suitsum = 0, j = 0;

	int rank2 = 0, pattern = 0;
	int flag = 0;

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);

	copyTable(copy_cards, my_cards);

	if (state.suit[0] == 1)
		ba_suitsum = ba_suitsum + 1;
	if (state.suit[1] == 1)
		ba_suitsum = ba_suitsum + 2;
	if (state.suit[2] == 1)
		ba_suitsum = ba_suitsum + 4;
	if (state.suit[3] == 1)
		ba_suitsum = ba_suitsum + 8;

	highCards(temp, copy_cards, state.ord); //場より強いカードを残す
	if (state.lock == 1)
	{								 //ロックされているとき
		lockCards(temp, state.suit); //出せないカードを除去
	}
	makeJGroupTable2(group, temp);
	//makeGroupTable(group,temp);
	nCards2(ngroup, group, state.qty);

	//outputCards(ngroup);

	if (qtyOfCards(ngroup) >= state.qty)
	{

		for (rank2 = state.ord + 1; rank2 < 14; rank2++)
		{
			for (pattern = 0; pattern < 6; pattern++)
			{

				copyTable(copy_cards, my_cards);

				Group2(temp2, copy_cards, ngroup, rank2, state.qty, pattern);

				if (qtyOfCards(temp2) == state.qty)
				{

					//outputCards(temp2);

					kumi_suitsum = get_suitsum(temp2);
					if (kumi_suitsum == ba_suitsum)
					{
						tsuyosa = setvalue_pair_shibari(temp2, kumi_suitsum);
					}
					else
					{
						tsuyosa = setvalue_pair2(temp2);
					}

					cardsDiff3(copy_cards, temp2);
					bunkatsu_pre2(copy_cards);

					//printf("tsuyosa %d\n",tsuyosa);
					//printf("state.kumi_info[13][0] %d\n",state.kumi_info[13][0]);
					//printf("state.kumi_info[13][6] %d\n",state.kumi_info[13][6]);

					if (state.kumi_info[13][0] - state.kumi_info[13][6] <= 1 && qtyOfCards5(temp2, 6) == 0)
					{
						copyTable(out_cards, temp2);
						flag = 1;
					}
				}
			}
			if (flag == 1)
				break;
		}
	}
}

void my_finish_followSequence3(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};

	int copy_cards[8][15] = {{0}};

	int tsuyosa = 0, ba_suit = 0, kumi_suit = 0, j = 0;
	int mark = 0, pattern = 0;

	int flag = 0;

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);

	copyTable(copy_cards, my_cards);

	if (state.suit[0] == 1)
		ba_suit = 0;
	if (state.suit[1] == 1)
		ba_suit = 1;
	if (state.suit[2] == 1)
		ba_suit = 2;
	if (state.suit[3] == 1)
		ba_suit = 3;

	highCards(temp, my_cards, state.ord); //場より強いカードを残す
	if (state.lock == 1)
	{								 //ロックされているとき
		lockCards(temp, state.suit); //出せないカードを除去
	}

	for (pattern = 0; pattern < 12; pattern++)
	{
		for (mark = 0; mark < 4; mark++)
		{

			copyCards(copy_cards, my_cards);

			Sequence2(temp2, temp, mark, state.qty, pattern);

			if (qtyOfCards(temp2) == state.qty && get_rank_min(temp2) > state.ord)
			{
				kumi_suit = get_suit(temp2);
				if (kumi_suit == ba_suit)
				{
					tsuyosa = setvalue_kaidan_shibari(temp2, kumi_suit);
				}
				else
				{
					tsuyosa = setvalue_kaidan2(temp2);
				}

				cardsDiff3(copy_cards, temp2);
				bunkatsu_pre2(copy_cards);

				if (state.kumi_info[13][0] - state.kumi_info[13][6] <= 1)
				{
					copyTable(out_cards, temp2);
					flag = 1;
				}
			}
		}
		if (flag == 1)
			break;
	}
}
/*
void my_finish_followSequence3(int out_cards[8][15],int my_cards[8][15],int joker_flag){

  int seq[8][15];
  int nseq[8][15];
 
 	int temp[8][15]={{0}}; 
	int temp2[8][15]={{0}};
	int copy_cards[8][15]={{0}};
	int copy_cards2[8][15]={{0}};
	int copy_cards3[8][15]={{0}};
	int copy_cards4[8][15]={{0}};
	
	int tsuyosa=0,ba_suit=0,kumi_suit=0,j=0;
	
  clearTable(out_cards);
  bunkatsu_pre2(my_cards);

  copyTable(copy_cards,my_cards);
  copyTable(copy_cards2,my_cards);
  copyTable(copy_cards3,my_cards);
  copyTable(copy_cards4,my_cards);
	
  if(state.suit[0]==1)ba_suit=0;
  if(state.suit[1]==1)ba_suit=1;
  if(state.suit[2]==1)ba_suit=2;
  if(state.suit[3]==1)ba_suit=3;
	
  highCards(temp,my_cards,state.ord);          //場より強いカードを残す
  if(state.lock==1){                           //ロックされているとき
    lockCards(temp,state.suit);                //出せないカードを除去
  }
  makeKaidanTable(seq,temp);                   //階段を書き出す
  nCards2(nseq,seq,state.qty);
 
	
  	 lowSequence(temp2,copy_cards,nseq);//弱いほうから1組
	 if(qtyOfCards(temp2)==state.qty){
	 	kumi_suit=get_suit(temp2);
		if(kumi_suit==ba_suit){
			tsuyosa=setvalue_kaidan_shibari(temp2,kumi_suit);
		}else{
			tsuyosa=setvalue_kaidan2(temp2);
		}
	 	
		cardsDiff3(copy_cards,temp2);
	 	bunkatsu_pre2(copy_cards);
		
	 	if(state.kumi_info[13][0]-state.kumi_info[13][6]<=1){
	 		copyTable(out_cards,temp2);
	 	}
	 }
	
	if(qtyOfCards(out_cards)==0){
		clearTable(temp2);
		highSequence(temp2,copy_cards2,nseq);//強いほうから1組
		if(qtyOfCards(temp2)==state.qty){
		 	kumi_suit=get_suit(temp2);
			if(kumi_suit==ba_suit){
				tsuyosa=setvalue_kaidan_shibari(temp2,kumi_suit);
			}else{
				tsuyosa=setvalue_kaidan2(temp2);
			}
		 	
			cardsDiff3(copy_cards2,temp2);
		 	bunkatsu_pre2(copy_cards2);
			
		 	if(state.kumi_info[13][0]-state.kumi_info[13][6]<=1){
		 		copyTable(out_cards,temp2);
		 	}
		}
	 }
	
	
	if(state.joker==1){
    //場と同じ枚数の階段が無いときジョーカーを使って探す
   	 	makeJKaidanTable(seq,temp);
   		nCards2(nseq,seq,state.qty);   
		
		if(qtyOfCards(out_cards)==0){	
			clearTable(temp2);
		    lowSequence(temp2,copy_cards3,nseq);//弱いほうから1組
			if(qtyOfCards(temp2)==state.qty){
			 	kumi_suit=get_suit(temp2);
				if(kumi_suit==ba_suit){
					tsuyosa=setvalue_kaidan_shibari(temp2,kumi_suit);
				}else{
					tsuyosa=setvalue_kaidan2(temp2);
				}
			 	
				cardsDiff3(copy_cards3,temp2);
			 	bunkatsu_pre2(copy_cards3);
				
			 	if(state.kumi_info[13][0]-state.kumi_info[13][6]<=1){
			 		copyTable(out_cards,temp2);
			 	}
		 	}
		}
		
		if(qtyOfCards(out_cards)==0){
			clearTable(temp2);
			highSequence(temp2,copy_cards4,nseq);//強いほうから1組
			if(qtyOfCards(temp2)==state.qty){
			 	kumi_suit=get_suit(temp2);
				if(kumi_suit==ba_suit){
					tsuyosa=setvalue_kaidan_shibari(temp2,kumi_suit);
				}else{
					tsuyosa=setvalue_kaidan2(temp2);
				}
			 	
				cardsDiff3(copy_cards4,temp2);
			 	bunkatsu_pre2(copy_cards4);
				
			 	if(state.kumi_info[13][0]-state.kumi_info[13][6]<=1){
			 		copyTable(out_cards,temp2);
			 	}
			}
		 }
 
 	 }
	
}
*/

void my_finish_follow4(int out_cards[8][15], int my_cards[8][15])
{

	clearTable(out_cards);

	if (state.qty == 1)
	{
		my_finish_followSolo4(out_cards, my_cards, state.joker); //一枚のとき
	}
	else
	{
		if (state.sequence == 0)
		{
			my_finish_followGroup4(out_cards, my_cards, state.joker); //枚数組のとき
		}
		else
		{
			my_finish_followSequence2(out_cards, my_cards, state.joker); //階段のとき
		}
	}
}

void my_finish_followSolo4(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};
	int copy_cards[8][15] = {{0}};
	int copy_cards2[8][15] = {{0}};
	int copy_cards3[8][15] = {{0}};

	int tsuyosa = 0, ba_suit = 0, kumi_suit = 0;

	int o_cards[8][15] = {{0}};
	int o_cards2[8][15] = {{0}};
	int flag = 0;
	int cards[8][15] = {{0}};
	int cards2[8][15] = {{0}};

	int tsuyosa2 = 0;
	int tsuyosa3 = 0;

	int flag3 = 0;
	int shibari = 0;

	int temp3[8][15] = {{0}};
	int temp4[8][15] = {{0}};

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);
	copyCards(copy_cards, my_cards);

	if (state.suit[0] == 1)
		ba_suit = 0;
	if (state.suit[1] == 1)
		ba_suit = 1;
	if (state.suit[2] == 1)
		ba_suit = 2;
	if (state.suit[3] == 1)
		ba_suit = 3;

	highCards(temp, copy_cards, state.ord);

	if (state.lock == 1)
	{
		lockCards(temp, state.suit);
	}

	while (qtyOfCards(temp) > 0)
	{ //&&flag3==0
		copyCards(copy_cards, my_cards);

		if (state.find_s3 == 0)
			joker_delete(temp);

		lowSolo2(temp2, temp, state.joker);

		kumi_suit = get_suit(temp2);
		if (kumi_suit == ba_suit)
		{
			tsuyosa = setvalue_single_shibari(temp2, kumi_suit);
		}
		else
		{
			tsuyosa = setvalue_single2(temp2);
		}

		cardsDiff3(temp, temp2);

		cardsDiff3(copy_cards, temp2);

		if (temp2[0][14] == 2 && copy_cards[0][1] == 1)
		{
			copy_cards[0][1] = 0;
			//printf("test///     %d\n",state.game_count);
		}

		copyCards(copy_cards2, copy_cards); //
		copyCards(copy_cards3, copy_cards); //

		//bunkatsu_pre2(copy_cards);

		//my_finish_followSolo4の変更点、ここから
		flag = 0; //リセット
		clearTable(o_cards);
		clearTable(o_cards2);
		clearTable(cards);
		clearTable(cards2);
		tsuyosa2 = 0;
		tsuyosa3 = 0;
		getField2(temp2); //「2」
		shibari = 0;
		/*
	follow_o_shibari(o_cards,state.rest_cards);
		if(kumi_suit!=ba_suit){
			follow_o2(o_cards2,state.rest_cards);
			shibari=1;
		}
	*/

		if (kumi_suit == ba_suit)
		{
			follow_o_shibari(o_cards, state.rest_cards);
			shibari = 1;
		}
		else
		{
			if (state.player_number == 2)
			{
				follow_o_shibari(o_cards, state.rest_cards); //両方行う。
			}
			follow_o2(o_cards2, state.rest_cards);
		}

		if (qtyOfCards(o_cards) == qtyOfCards(state.rest_cards))
			flag = 99;
		if (qtyOfCards(o_cards2) == qtyOfCards(state.rest_cards))
			flag = 99;
		/*
		printf("temp2\n");
		outputCards(temp2);
		printf("state.ord2          %d\n",state.ord2);
		printf("state.sequence2     %d\n",state.sequence2);
		printf("state.qty2          %d\n",state.qty2);
		outputCards(o_cards);
		outputCards(o_cards2);
		*/

		if (qtyOfCards(o_cards) == 0)
		{
			flag++;
			tsuyosa2 = STRONG;
		}
		else
		{
			getField2(o_cards);
			follow_o_shibari(cards, copy_cards);
			if (qtyOfCards(cards) != 0)
			{
				flag++;
				kumi_suit = get_suit(cards);
				tsuyosa2 = setvalue_single_shibari(cards, kumi_suit);
			}
		}

		if (qtyOfCards(o_cards2) == 0)
		{
			flag++;
			tsuyosa3 = STRONG;
		}
		else
		{
			getField2(o_cards2);
			follow_o2(cards2, copy_cards);
			if (qtyOfCards(cards2) != 0)
			{
				flag++;
				tsuyosa3 = setvalue_single2(cards2);
			}
		}

		if (flag == 2 && tsuyosa2 >= STRONG && tsuyosa3 >= STRONG)
		{
			cardsDiff3(copy_cards2, cards);
			bunkatsu_pre2(copy_cards2);
		}

		if (flag == 2 && state.kumi_info[13][0] - state.kumi_info[13][6] <= 1 && tsuyosa2 >= STRONG && tsuyosa3 >= STRONG)
		{

			cardsDiff3(copy_cards3, cards2);
			bunkatsu_pre2(copy_cards3);

			if (state.kumi_info[13][0] - state.kumi_info[13][6] <= 1)
			{

				copyTable(temp3, temp2);
				//flag3=1;

				if (shibari == 1)
				{
					copyTable(temp4, temp2);
				}
				/*
						printf("my_finish_followsolo4     %d\n",state.game_count);
						printf("temp2\n");
						outputCards(temp2);
						printf("o_cards\n");
						outputCards(o_cards);
						outputCards(cards);
						printf("o_cards2\n");
						outputCards(o_cards2);
						outputCards(cards2);
						printf("kumi_suit   %d\n",kumi_suit);
						printf("ba_suit   %d\n",ba_suit);
						*/

				/*	
						outputkumi_info(0);
						outputkumi_info(1);
						outputkumi_info(2);
						outputkumi_info(3);
						outputkumi_info(4);
						outputkumi_info(5);
						*/
			}
		}
	}
	/*
	if(qtyOfCards6(state.rest_cards,ba_suit,6)==1&&get_rank_max(temp2)<6){
		
	}else{
		copyTable(out_cards,temp4);
	}
	*/
	copyTable(out_cards, temp4);

	if (beEmptyCards(out_cards) == 1)
	{
		copyTable(out_cards, temp3);
	}
}

void my_finish_followGroup4(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int group[8][15];
	int ngroup[8][15];

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};
	int copy_cards[8][15] = {{0}};
	int copy_cards2[8][15] = {{0}};
	int copy_cards3[8][15] = {{0}};

	int tsuyosa = 0, ba_suitsum = 0, kumi_suitsum = 0, j = 0;

	int rank2 = 0, pattern = 0;
	int flag = 0;

	int o_cards[8][15] = {{0}};
	int o_cards2[8][15] = {{0}};
	int flag2 = 0;
	int cards[8][15] = {{0}};
	int cards2[8][15] = {{0}};

	int tsuyosa2 = 0;
	int tsuyosa3 = 0;
	int shibari = 0;

	int temp3[8][15] = {{0}};
	int temp4[8][15] = {{0}};

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);

	copyTable(copy_cards, my_cards);

	if (state.suit[0] == 1)
		ba_suitsum = ba_suitsum + 1;
	if (state.suit[1] == 1)
		ba_suitsum = ba_suitsum + 2;
	if (state.suit[2] == 1)
		ba_suitsum = ba_suitsum + 4;
	if (state.suit[3] == 1)
		ba_suitsum = ba_suitsum + 8;

	highCards(temp, copy_cards, state.ord); //場より強いカードを残す
	if (state.lock == 1)
	{								 //ロックされているとき
		lockCards(temp, state.suit); //出せないカードを除去
	}
	makeJGroupTable2(group, temp); //残ったものから枚数組を書き出す
								   //makeGroupTable(group,temp);
	nCards2(ngroup, group, state.qty);

	//outputCards(ngroup);

	if (qtyOfCards(ngroup) >= state.qty)
	{

		for (rank2 = state.ord + 1; rank2 < 14; rank2++)
		{
			//for(rank2=13;rank2>state.ord;rank2--){//大きい数字から検討
			for (pattern = 0; pattern < 6; pattern++)
			{

				copyTable(copy_cards, my_cards);

				Group2(temp2, copy_cards, ngroup, rank2, state.qty, pattern);

				if (qtyOfCards(temp2) == state.qty)
				{

					//outputCards(temp2);

					kumi_suitsum = get_suitsum(temp2);
					if (kumi_suitsum == ba_suitsum)
					{
						tsuyosa = setvalue_pair_shibari(temp2, kumi_suitsum);
					}
					else
					{
						tsuyosa = setvalue_pair2(temp2);
					}

					cardsDiff3(copy_cards, temp2);

					copyCards(copy_cards2, copy_cards); //
					copyCards(copy_cards3, copy_cards); //

					//bunkatsu_pre2(copy_cards);

					//my_finish_followGroup4の変更点、ここから
					flag2 = 0; //リセット
					clearTable(o_cards);
					clearTable(o_cards2);
					clearTable(cards);
					clearTable(cards2);
					tsuyosa2 = 0;
					tsuyosa3 = 0;
					shibari = 0;

					getField2(temp2); //「2」

					if (kumi_suitsum == ba_suitsum)
					{
						follow_o_shibari(o_cards, state.rest_cards);
						shibari = 1;
					}
					else
					{
						if (state.player_number == 2)
						{
							follow_o_shibari(o_cards, state.rest_cards); //両方行う。
						}
						follow_o2(o_cards2, state.rest_cards);
					}

					if (state.player_number == 2)
					{
						if (qtyOfCards(o_cards) == qtyOfCards(state.rest_cards))
							flag = 99;
						if (qtyOfCards(o_cards2) == qtyOfCards(state.rest_cards))
							flag = 99;
					}
					/*
						printf("temp2\n");
						outputCards(temp2);
						printf("state.ord2          %d\n",state.ord2);
						printf("state.sequence2     %d\n",state.sequence2);
						printf("state.qty2          %d\n",state.qty2);
						outputCards(o_cards);
						outputCards(o_cards2);
						*/

					if (qtyOfCards(o_cards) == 0)
					{
						flag2++;
						tsuyosa2 = STRONG;
					}
					else
					{
						getField2(o_cards);
						follow_o_shibari(cards, copy_cards);
						if (qtyOfCards(cards) != 0)
						{
							flag2++;
							kumi_suitsum = get_suitsum(cards);
							tsuyosa2 = setvalue_pair_shibari(cards, kumi_suitsum);
						}
					}

					if (qtyOfCards(o_cards2) == 0)
					{
						flag2++;
						tsuyosa3 = STRONG;
					}
					else
					{
						getField2(o_cards2);
						follow_o2(cards2, copy_cards);
						if (qtyOfCards(cards2) != 0)
						{
							flag2++;
							tsuyosa3 = setvalue_pair2(cards2);
						}
					}

					if (flag2 == 2 && tsuyosa2 >= STRONG && tsuyosa3 >= STRONG)
					{
						cardsDiff3(copy_cards2, cards);
						bunkatsu_pre2(copy_cards2);
					}

					if (flag2 == 2 && state.kumi_info[13][0] - state.kumi_info[13][6] <= 1 && tsuyosa2 >= STRONG && tsuyosa3 >= STRONG)
					{

						cardsDiff3(copy_cards3, cards2);
						bunkatsu_pre2(copy_cards3);

						if (state.kumi_info[13][0] - state.kumi_info[13][6] <= 1)
						{

							copyTable(temp3, temp2);

							if (shibari == 1)
							{
								copyTable(temp4, temp2);
							}
							/*
											printf("my_finish_followsolo4     %d\n",state.game_count);
											printf("temp2\n");
											outputCards(temp2);
											printf("o_cards\n");
											outputCards(o_cards);
											outputCards(cards);
											printf("o_cards2\n");
											outputCards(o_cards2);
											outputCards(cards2);
											
											outputkumi_info(0);
											outputkumi_info(1);
											outputkumi_info(2);
											outputkumi_info(3);
											outputkumi_info(4);
											outputkumi_info(5);
											*/
						}
					}
				}
			}
		}
	}
	copyTable(out_cards, temp4);

	if (beEmptyCards(out_cards) == 1)
	{
		copyTable(out_cards, temp3);
	}
}

void my_finish_follow5(int out_cards[8][15], int my_cards[8][15])
{

	clearTable(out_cards);

	if (state.qty == 1)
	{
		//my_finish_followSolo5(out_cards,my_cards,state.joker);    //一枚のとき
	}
	else
	{
		if (state.sequence == 0)
		{
			my_finish_followGroup5(out_cards, my_cards, state.joker); //枚数組のとき
		}
		else
		{
			my_finish_followSequence5(out_cards, my_cards, state.joker); //階段のとき
		}
	}
}

void my_finish_followSolo5(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};
	int copy_cards[8][15] = {{0}};

	int tsuyosa = 0, ba_suit = 0, kumi_suit = 0;

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);
	copyCards(copy_cards, my_cards);

	if (state.suit[0] == 1)
		ba_suit = 0;
	if (state.suit[1] == 1)
		ba_suit = 1;
	if (state.suit[2] == 1)
		ba_suit = 2;
	if (state.suit[3] == 1)
		ba_suit = 3;

	highCards(temp, copy_cards, state.ord);

	if (state.lock == 1)
	{
		lockCards(temp, state.suit);
	}

	while (qtyOfCards(temp) > 0 && beEmptyCards(out_cards) == 1)
	{
		copyCards(copy_cards, my_cards);

		if (state.find_s3 == 0)
			joker_delete(temp);

		lowSolo2(temp2, temp, state.joker);

		kumi_suit = get_suit(temp2);
		if (kumi_suit == ba_suit)
		{
			tsuyosa = setvalue_single_shibari(temp2, kumi_suit);
		}
		else
		{
			tsuyosa = setvalue_single2(temp2);
		}

		cardsDiff3(temp, temp2);
		cardsDiff3(copy_cards, temp2);

		if (temp2[0][14] == 2 && copy_cards[0][1] == 1)
		{
			copy_cards[0][1] = 0;
			//printf("test/     %d\n",state.game_count);
		}
		state.cause_rev = 1;
		bunkatsu_pre2(copy_cards);
		state.cause_rev = 0;

		if (tsuyosa >= STRONG - 1 && state.kumi_info[13][0] - state.kumi_info[13][6] <= 1)
			copyTable(out_cards, temp2);

		/*
		printf("my_cards\n");
		outputCards(my_cards);
		printf("temp\n");
		outputCards(temp);
		printf("temp2\n");
		outputCards(temp2);
		*/
	}
}

void my_finish_followGroup5(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int group[8][15];
	int ngroup[8][15];

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};
	int copy_cards[8][15] = {{0}};

	int tsuyosa = 0, ba_suitsum = 0, kumi_suitsum = 0, j = 0;

	int rank2 = 0, pattern = 0;
	int flag = 0;

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);

	copyTable(copy_cards, my_cards);

	if (state.suit[0] == 1)
		ba_suitsum = ba_suitsum + 1;
	if (state.suit[1] == 1)
		ba_suitsum = ba_suitsum + 2;
	if (state.suit[2] == 1)
		ba_suitsum = ba_suitsum + 4;
	if (state.suit[3] == 1)
		ba_suitsum = ba_suitsum + 8;

	highCards(temp, copy_cards, state.ord); //場より強いカードを残す
	if (state.lock == 1)
	{								 //ロックされているとき
		lockCards(temp, state.suit); //出せないカードを除去
	}
	makeJGroupTable2(group, temp); //残ったものから枚数組を書き出す
	nCards2(ngroup, group, state.qty);
	/*
	printf("state.suit[0]     %d\n",state.suit[0]);
	printf("state.suit[1]     %d\n",state.suit[1]);
	printf("state.suit[2]     %d\n",state.suit[2]);
	printf("state.suit[3]     %d\n",state.suit[3]);
	*/
	/*
	printf("temp\n");
	outputCards(temp);
	printf("group\n");
	outputCards(group);
	printf("ngroup\n");
	outputCards(ngroup);
	*/

	if (qtyOfCards(ngroup) >= state.qty)
	{

		for (rank2 = state.ord + 1; rank2 < 14; rank2++)
		{
			for (pattern = 0; pattern < 6; pattern++)
			{

				copyTable(copy_cards, my_cards);

				Group2(temp2, copy_cards, ngroup, rank2, state.qty, pattern);

				if (qtyOfCards(temp2) == state.qty)
				{

					//outputCards(temp2);

					kumi_suitsum = get_suitsum(temp2);
					if (kumi_suitsum == ba_suitsum)
					{
						tsuyosa = setvalue_pair_shibari3(temp2, kumi_suitsum);
					}
					else
					{
						tsuyosa = setvalue_pair3(temp2);
					}

					cardsDiff3(copy_cards, temp2);
					state.cause_rev = 1;
					bunkatsu_pre2(copy_cards);
					state.cause_rev = 0;

					//printf("tsuyosa %d\n",tsuyosa);
					//printf("state.kumi_info[13][0] %d\n",state.kumi_info[13][0]);
					//printf("state.kumi_info[13][6] %d\n",state.kumi_info[13][6]);
					/*
					printf("tsuyosa   %d\n",tsuyosa);
					printf("temp2\n");
					outputCards(temp2);
					*/

					if (tsuyosa >= STRONG * state.qty - 1 && state.kumi_info[13][0] - state.kumi_info[13][6] <= 1)
					{
						copyTable(out_cards, temp2);
						flag = 1;
					}
				}
			}
			if (flag == 1)
				break;
		}
	}
}

void my_finish_followSequence5(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};

	int copy_cards[8][15] = {{0}};

	int tsuyosa = 0, ba_suit = 0, kumi_suit = 0, j = 0;
	int mark = 0, pattern = 0;

	int flag = 0;

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);

	copyTable(copy_cards, my_cards);

	if (state.suit[0] == 1)
		ba_suit = 0;
	if (state.suit[1] == 1)
		ba_suit = 1;
	if (state.suit[2] == 1)
		ba_suit = 2;
	if (state.suit[3] == 1)
		ba_suit = 3;

	highCards(temp, my_cards, state.ord); //場より強いカードを残す
	if (state.lock == 1)
	{								 //ロックされているとき
		lockCards(temp, state.suit); //出せないカードを除去
	}

	for (pattern = 0; pattern < 12; pattern++)
	{
		for (mark = 0; mark < 4; mark++)
		{

			copyCards(copy_cards, my_cards);

			Sequence2(temp2, temp, mark, state.qty, pattern);

			if (qtyOfCards(temp2) == state.qty && get_rank_min(temp2) > state.ord)
			{
				kumi_suit = get_suit(temp2);
				if (kumi_suit == ba_suit)
				{
					tsuyosa = setvalue_kaidan_shibari3(temp2, kumi_suit);
				}
				else
				{
					tsuyosa = setvalue_kaidan3(temp2);
				}

				cardsDiff3(copy_cards, temp2);
				state.cause_rev = 1;
				bunkatsu_pre2(copy_cards);
				state.cause_rev = 0;

				if (tsuyosa >= STRONG * state.qty - 1 && state.kumi_info[13][0] - state.kumi_info[13][6] <= 1)
				{
					copyTable(out_cards, temp2);
					flag = 1;
				}
			}
		}
		if (flag == 1)
			break;
	}
}

void my_follow3(int out_cards[8][15], int my_cards[8][15])
{

	clearTable(out_cards);

	state.tsuyosa = 0;

	if (state.qty == 1)
	{
		my_followSolo3(out_cards, my_cards, state.joker); //一枚のとき
	}
	else
	{
		if (state.sequence == 0)
		{
			my_followGroup3(out_cards, my_cards, state.joker); //枚数組のとき
		}
		else
		{
			my_followSequence3(out_cards, my_cards, state.joker); //階段のとき
		}
	}
}

void my_followSolo3(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};
	int temp4[8][15] = {{0}};
	int temp5[8][15] = {{0}};
	int temp6[8][15] = {{0}};
	int temp7[8][15] = {{0}};
	int temp8[8][15] = {{0}};
	int copy_cards[8][15] = {{0}};

	int tsuyosa = 0, ba_suit = 0, kumi_suit = 0;

	int handvalue = 0;

	int h = 4;
	int nagashi_kumi = 0, nagashi_kumi2 = 0, nagashi_flag = 0;
	int kumisuu = 0, kumisuu2 = 0;

	int flag = 0;
	int tsuyosa2 = 0;
	int kakunin = 0;

	int hh = 15, x = 0, y = 0;

	int kumi_rank = 0;
	int kumi_rank_s = 0;
	int shibari_flag = 0;

	int flag_sh = 0;
	int rest_rank_max = 0;
	int nagashi_kumi_1 = 0, nagashi_kumi_2 = 0;

	int shibari_sa = 0;
	int start_handvalue = 0;
	int handvalue2 = 0;
	int handvalue3 = 0;
	int handvalue4 = 0;
	int nagashi_kumi100 = 0;
	int nagashi_kumi100_2 = 0;

	int flag2 = 0;
	int kumi_suit_s = 9;
	int flag3 = 0;
	int flag4 = 0;

	int sonomark = 99;

	rest_rank_max = get_rank_max(state.rest_cards);

	clearTable(out_cards);
	copyCards(copy_cards, my_cards);
	bunkatsu_pre2(my_cards);

	/*
	if(state.player_number>2){
		handvalue=state.kumi_info[13][h]-1;
	}else{
		//handvalue=state.kumi_info[13][h]-10;
		handvalue=0;
	}
	*/
	if (state.player_number >= 4)
		handvalue = state.kumi_info[13][h] - 1;
	if (state.player_number == 3)
		handvalue = state.kumi_info[13][h] - 1;
	if (state.player_number == 2)
		handvalue = 0;

	start_handvalue = handvalue;

	nagashi_kumi = state.kumi_info[13][35];
	kumisuu = state.kumi_info[13][0];
	nagashi_kumi100 = state.kumi_info[13][6];

	if (state.suit[0] == 1)
		ba_suit = 0;
	if (state.suit[1] == 1)
		ba_suit = 1;
	if (state.suit[2] == 1)
		ba_suit = 2;
	if (state.suit[3] == 1)
		ba_suit = 3;

	highCards(temp, copy_cards, state.ord);

	if (state.lock == 1)
	{
		lockCards(temp, state.suit);
	}

	while (qtyOfCards(temp) > 0)
	{ //&&beEmptyCards(out_cards)==1

		//printf("handvalue3   %d\n",handvalue);

		copyCards(copy_cards, my_cards);

		kumi_rank = 0;
		shibari_flag = 0;
		shibari_sa = 0;

		if (state.find_s3 == 0)
			joker_delete(temp);

		//if(qtyOfCards5(my_cards,6)<2){
		//lowSolo2(temp2,temp,state.joker);
		//}else{
		//lowSolo2_1(temp2,temp,state.joker);
		//printf("test game_count   %d\n",state.game_count);
		//}
		/*
		if(kumisuu>7){
			lowSolo2_1(temp2,temp,state.joker);
		}else{
		*/
		/*
		if(qtyOfCards5(my_cards,6)>=2&&kumisuu>7){
  	 		lowSolo2_2(temp2,temp,state.joker);
		}
		*/
		if (qtyOfCards5(my_cards, 6) >= 2 || kumisuu >= 10)
		{ //革命時verにはない
			lowSolo2_1(temp2, temp, state.joker);
		}
		else
		{
			lowSolo2(temp2, temp, state.joker);
		}

		kumi_suit = get_suit(temp2);
		kumi_rank = get_rank_min(temp2);

		//printf("handvalue4   %d\n",handvalue);
		if (kumi_suit == ba_suit)
		{
			kakunin = qtyOfCards11(my_cards, kumi_suit);

			flag = 0;
			flag2 = 0;
			flag4 = 0;

			tsuyosa = setvalue_single_shibari(temp2, kumi_suit);
			tsuyosa2 = setvalue_single2(temp2);

			if (kakunin != 0)
			{ //&&qtyOfCards6(state.submitted_cards_plus_hands,kumi_suit,6)==1//&&kakunin!=13//&&kakunin!=kumi_rank
				//&&kakunin!=kumi_rank
				/*
				if(kakunin==13){
					clearTable(temp8);
					lowCards2(temp8,copy_cards,kumi_rank,kumi_suit);
					bunkatsu_pre2(temp8);
					if(state.kumi_info[13][0]-state.kumi_info[13][35]<=2||kumi_rank_s==0||kumi_rank<=kumi_rank_s+3)flag2=1;
				}
				
				if(kakunin!=13){
					clearTable(temp8);
					lowCards2(temp8,copy_cards,kumi_rank,kumi_suit);
					bunkatsu_pre2(temp8);
					if(state.kumi_info[13][0]-state.kumi_info[13][35]<=2||kumi_rank_s==0||kumi_rank<=kumi_rank_s+4)flag2=1;
				}
				*/

				flag2 = 1;

				if (qtyOfCards8(state.rest_cards, kakunin) > 0)
					flag4 = 1;

				if (flag2 == 1)
				{
					if (state.player_number > 2)
					{
						if (tsuyosa2 < tsuyosa)
						{
							if (kumi_rank <= kumi_rank_s + 0 || kumi_rank_s == 0 || (flag4 == 1 && kumi_rank <= kumi_rank_s + 4))
							{ //
								tsuyosa = tsuyosa2 + 1;
							}
							else
							{
								tsuyosa = tsuyosa2;
							}
						}
						else
						{
							tsuyosa = tsuyosa2;
						}
					}
					else
					{
						tsuyosa = tsuyosa2;
					}

					if (tsuyosa > STRONG)
						tsuyosa = STRONG;

					if (state.player_number > 2)
						shibari_flag = 1; //

					flag = 1;

					shibari_sa = tsuyosa - tsuyosa2;
				}
			}
			else
			{
				tsuyosa = tsuyosa2;
			}
			/*
			if(flag==0){
				if(tsuyosa==STRONG){
					if(tsuyosa2<tsuyosa){
						if(kumi_rank_s==0||flag3==0){
							if(kumi_rank<=kumi_rank_s+2||kumi_rank_s==0){
								tsuyosa=tsuyosa2+1;
							}else{
								tsuyosa=tsuyosa2;
							}
						}else{
							tsuyosa=tsuyosa2;
						}
					}else{
						tsuyosa=tsuyosa2;
					}
					shibari_sa=tsuyosa-tsuyosa2;
				}else{
					tsuyosa=tsuyosa2;
				}
			}
			*/
		}
		else
		{
			tsuyosa = setvalue_single2(temp2);
		}

		//printf("handvalue5   %d\n",handvalue);

		cardsDiff3(temp, temp2);
		cardsDiff3(copy_cards, temp2);
		bunkatsu_pre2(copy_cards);

		if (tsuyosa >= STRONG - 1)
		{
			nagashi_flag = 1;
		}
		else
		{
			nagashi_flag = 0;
		}
		nagashi_kumi2 = state.kumi_info[13][35];
		kumisuu2 = state.kumi_info[13][0];
		nagashi_kumi100_2 = state.kumi_info[13][6];

		//y=(state.kumi_info[13][hh]+tsuyosa)/(state.kumi_info[13][0]+1);
		/*
		printf("handvalue   %d\n",state.kumi_info[13][h]);
		printf("tsuyosa     %d\n",tsuyosa);
		printf("temp2\n");
		outputCards(temp2);
		printf("state.kumi_info[13][15]                   %d\n",state.kumi_info[13][15]);
		printf("state.kumi_info[13][0]                    %d\n",state.kumi_info[13][0]);
		printf("state.kumi_info[13][15]+tsuyosa/(kumisuu+1)   %d\n",(state.kumi_info[13][15]+tsuyosa)/(state.kumi_info[13][0]+1));
		*/

		//printf("handvalue6   %d\n",handvalue);
		/*
		printf("game_count   %d\n",state.game_count);
		printf("tsuyosa     %d\n",tsuyosa);
		printf("state.kumi_info[13][h]   %d\n",state.kumi_info[13][h]);
		printf("handvalue   %d\n",handvalue);
		//printf("y   %d\n",y);
		//printf("x   %d\n",x);
		printf("temp2\n"); 
		outputCards(temp2);
		*/

		//outputkumi_info2(state.kumi_info);
		/*
		outputkumi_info(0);
		outputkumi_info(1);
		outputkumi_info(2);
		outputkumi_info(3);
		outputkumi_info(4);
		outputkumi_info(5);
		outputkumi_info(6);
		outputkumi_info(7);
		outputkumi_info(8);
		outputkumi_info(9);
		*/

		if (state.player_number > 2)
		{

			/*
			if(tsuyosa+state.kumi_info[13][h]==handvalue&&y>=x&&nagashi_kumi2+nagashi_flag>=nagashi_kumi&&kumi_rank==kumi_rank_s&&(kumi_suit==qtyOfCards14(state.submitted_cards,kumi_rank)||qtyOfCards12(state.rest_cards,kumi_rank,kumi_suit)<2)&&(kumi_rank==6||kumi_rank==13)){//&&nagashi_kumi2+1>=nagashi_kumi//&&kumisuu2-nagashi_kumi2<=kumisuu-nagashi_kumi//&&y>=x
				//&&nagashi_kumi2+nagashi_flag>=nagashi_kumi
				handvalue=tsuyosa+state.kumi_info[13][h];
				copyTable(out_cards,temp2);
				state.tsuyosa=tsuyosa;
				x=y;
				kumi_rank_s=kumi_rank;
				kumi_suit_s=kumi_suit;
				flag3=flag2;
				printf("mark_ishiki\n");
				//sonomark=qtyOfCards12(state.rest_cards,kumi_rank,kumi_suit);
			}
			*/
			//&&y>=x
			if (tsuyosa + state.kumi_info[13][h] > handvalue && nagashi_kumi2 + nagashi_flag >= nagashi_kumi)
			{ //&&nagashi_kumi2+1>=nagashi_kumi//&&kumisuu2-nagashi_kumi2<=kumisuu-nagashi_kumi//&&y>=x
				//&&nagashi_kumi2+nagashi_flag>=nagashi_kumi
				handvalue = tsuyosa + state.kumi_info[13][h];
				copyTable(out_cards, temp2);
				state.tsuyosa = tsuyosa;
				x = y;
				kumi_rank_s = kumi_rank;
				kumi_suit_s = kumi_suit;
				flag3 = flag2;
			}
		}
		else
		{
			if (tsuyosa + state.kumi_info[13][h] > handvalue)
			{ //&&nagashi_kumi2+1>=nagashi_kumi//&&kumisuu2-nagashi_kumi2<=kumisuu-nagashi_kumi

				handvalue = tsuyosa + state.kumi_info[13][h];
				copyTable(out_cards, temp2);
				state.tsuyosa = tsuyosa;
				kumi_rank_s = kumi_rank;
				kumi_suit_s = kumi_suit;
				flag3 = flag2;
			}
		}
		/*
		if(state.player_number<=4&&state.kumi_info[13][0]-nagashi_kumi100_2<kumisuu-nagashi_kumi&&nagashi_kumi100_2>=nagashi_kumi&&tsuyosa+state.kumi_info[13][h]>handvalue2&&tsuyosa>20&&state.kumi_info[13][0]-nagashi_kumi100_2<=1){
			handvalue2=tsuyosa+state.kumi_info[13][h];
			copyTable(temp4,temp2);
		}
		
		if(state.player_number<=4&&state.kumi_info[13][0]-nagashi_kumi100_2<=kumisuu-nagashi_kumi&&nagashi_kumi100_2>=nagashi_kumi&&tsuyosa+state.kumi_info[13][h]>handvalue2&&tsuyosa>20&&state.kumi_info[13][0]-nagashi_kumi100_2<=1){
			handvalue3=tsuyosa+state.kumi_info[13][h];
			copyTable(temp5,temp2);
		}
		
		if(state.player_number<=4&&state.kumi_info[13][0]-nagashi_kumi100_2<kumisuu-nagashi_kumi&&nagashi_kumi100_2>=nagashi_kumi&&tsuyosa+state.kumi_info[13][h]>handvalue2&&tsuyosa>20){
			handvalue4=tsuyosa+state.kumi_info[13][h];
			copyTable(temp6,temp2);
		}
		*/
		if (temp2[0][14] == 2 && copy_cards[0][1] == 1)
		{
			copyTable(temp7, temp2);
		}
	}
	/*
	if(beEmptyCards(out_cards)==1&&beEmptyCards(temp4)==0){
		copyTable(out_cards,temp4);
		printf("temp4                     %d\n",state.game_count);
		outputCards(temp4);
	}
	
	if(beEmptyCards(out_cards)==1&&beEmptyCards(temp6)==0){
		copyTable(out_cards,temp6);
		printf("temp6                     %d\n",state.game_count);
		outputCards(temp6);
	}
	*/
	/*
	if(beEmptyCards(out_cards)==1&&beEmptyCards(temp5)==0){
		copyTable(out_cards,temp5);
		printf("temp5                     %d\n",state.game_count);
		outputCards(temp5);
	}
	*/

	if (beEmptyCards(out_cards) == 1 && beEmptyCards(temp7) == 0)
	{ //&&state.lock==0

		copyCards(copy_cards, my_cards);
		copy_cards[4][1] = 0;
		copy_cards[0][1] = 0;
		bunkatsu_pre2(copy_cards);
		if (state.kumi_info[13][0] - state.kumi_info[13][6] <= kumisuu - nagashi_kumi)
		{
			copyTable(out_cards, temp7);
			printf("temp7.1                    %d\n", state.game_count);
			outputCards(temp7);
		}
	}
	/*
	if(qtyOfCards8(state.rest_cards,get_rank_max(out_cards))==0&&get_suit(out_cards)!=ba_suit&&beEmptyCards(temp7)==0){
		
		copyCards(copy_cards,my_cards);
		copy_cards[4][1]=0;
		copy_cards[0][1]=0;
		bunkatsu_pre2(copy_cards);
		if(state.kumi_info[13][0]-state.kumi_info[13][6]<kumisuu-nagashi_kumi){
				copyTable(out_cards,temp7);
				printf("temp7.2                    %d\n",state.game_count);
				outputCards(temp7);
		}
		
	}
	*/
}

void my_followGroup3(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int group[8][15];
	int ngroup[8][15];

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};
	int copy_cards[8][15] = {{0}};

	int tsuyosa = 0, ba_suitsum = 0, kumi_suitsum = 0, j = 0;
	int handvalue = 0;

	int h = 4;
	int rank2 = 0, pattern = 0;
	int nagashi_kumi = 0, nagashi_kumi2 = 0, nagashi_flag = 0;
	int kumisuu = 0, kumisuu2 = 0;
	int tsuyosa2 = 0;
	int hh = 15, x = 0, y = 0;

	int kumi_rank = 0;
	int kumi_rank_s = 0;
	int shibari_flag = 0;

	int flag_sh = 0;

	int rest_rank_max = 0;
	int flag = 0;
	//int kumi_suitsum_s=99;

	rest_rank_max = get_rank_max(state.rest_cards);

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);
	if (state.player_number > 2)
	{
		handvalue = state.kumi_info[13][h] - 1;
	}
	else
	{
		handvalue = 0;
	}
	nagashi_kumi = state.kumi_info[13][35];
	kumisuu = state.kumi_info[13][0];

	//printf("handvalue   %d\n",state.kumi_info[13][h]-1);

	copyTable(copy_cards, my_cards);

	if (state.suit[0] == 1)
		ba_suitsum = ba_suitsum + 1;
	if (state.suit[1] == 1)
		ba_suitsum = ba_suitsum + 2;
	if (state.suit[2] == 1)
		ba_suitsum = ba_suitsum + 4;
	if (state.suit[3] == 1)
		ba_suitsum = ba_suitsum + 8;

	highCards(temp, copy_cards, state.ord); //場より強いカードを残す
	if (state.lock == 1)
	{								 //ロックされているとき
		lockCards(temp, state.suit); //出せないカードを除去
	}
	makeJGroupTable2(group, temp); //残ったものから枚数組を書き出す
	//makeGroupTable(group,temp);
	nCards2(ngroup, group, state.qty);

	if (qtyOfCards(ngroup) >= state.qty)
	{

		for (rank2 = state.ord + 1; rank2 < 14; rank2++)
		{
			for (pattern = 0; pattern < 6; pattern++)
			{

				copyCards(copy_cards, my_cards);

				Group2(temp2, copy_cards, ngroup, rank2, state.qty, pattern);

				if (qtyOfCards(temp2) == state.qty)
				{

					kumi_rank = 0;
					shibari_flag = 0;
					flag = 0;

					kumi_suitsum = get_suitsum(temp2);
					kumi_rank = get_rank_min(temp2);
					if (kumi_suitsum == ba_suitsum)
					{
						tsuyosa = setvalue_pair_shibari3(temp2, kumi_suitsum);
						tsuyosa2 = setvalue_pair3(temp2);
						if (state.player_number > 2)
						{
							/*
							if(tsuyosa<STRONG*state.qty&&tsuyosa2!=tsuyosa&&flag==0){
								tsuyosa=tsuyosa2+state.qty-1;
								flag=1;
							}
							*/
							/*
							if(tsuyosa<STRONG*state.qty&&tsuyosa2!=tsuyosa&&flag==0){
								tsuyosa=tsuyosa2;
								flag=1;
							}
							*/

							if (tsuyosa == STRONG * state.qty)
							{
								if (tsuyosa2 < tsuyosa)
								{
									if (kumi_rank <= kumi_rank_s + 2 || kumi_rank_s == 0)
									{
										tsuyosa = tsuyosa2 + 1;
									}
									else
									{
										tsuyosa = tsuyosa2;
									}
								}
								else
								{
									tsuyosa = tsuyosa2;
								}
							}
							else
							{
								tsuyosa = tsuyosa2;
							}

							shibari_flag = 1;
						}
						else
						{
							if (tsuyosa < STRONG * state.qty && tsuyosa2 != tsuyosa)
								tsuyosa = tsuyosa2;
						}
						if (tsuyosa > STRONG * state.qty)
							tsuyosa = STRONG * state.qty;
					}
					else
					{
						tsuyosa = setvalue_pair3(temp2);
					}

					cardsDiff3(copy_cards, temp2);
					bunkatsu_pre2(copy_cards);

					if (tsuyosa >= (STRONG - 1) * state.qty)
					{
						nagashi_flag = 1;
					}
					else
					{
						nagashi_flag = 0;
					}
					nagashi_kumi2 = state.kumi_info[13][35];
					kumisuu2 = state.kumi_info[13][0];

					/*
				printf("temp2\n");
				outputCards(temp2);
				printf("tsuyosa   %d\n",tsuyosa);
				printf("state.kumi_info[13][h]   %d\n",state.kumi_info[13][h]);
				printf("handvalue   %d\n",handvalue);
				*/

					//y=(state.kumi_info[13][hh]+(tsuyosa/state.qty))/(state.kumi_info[13][0]+1);
					//&&y>=x
					if (state.player_number > 2)
					{
						if (tsuyosa + state.kumi_info[13][h] == handvalue && nagashi_kumi2 + nagashi_flag >= nagashi_kumi && kumi_rank == kumi_rank_s && kumi_suitsum == ba_suitsum)
						{ //&&nagashi_kumi2+1>=nagashi_kumi&&kumisuu2-nagashi_kumi2<=kumisuu-nagashi_kumi
							//&&nagashi_kumi2+nagashi_flag>=nagashi_kumi
							handvalue = tsuyosa + state.kumi_info[13][h];
							copyTable(out_cards, temp2);
							state.tsuyosa = tsuyosa;
							x = y;
							kumi_rank_s = kumi_rank;
						}
						if (tsuyosa + state.kumi_info[13][h] > handvalue && nagashi_kumi2 + nagashi_flag >= nagashi_kumi)
						{ //&&nagashi_kumi2+1>=nagashi_kumi&&kumisuu2-nagashi_kumi2<=kumisuu-nagashi_kumi
							//&&nagashi_kumi2+nagashi_flag>=nagashi_kumi
							handvalue = tsuyosa + state.kumi_info[13][h];
							copyTable(out_cards, temp2);
							state.tsuyosa = tsuyosa;
							x = y;
							kumi_rank_s = kumi_rank;
						}
					}
					else
					{
						if (tsuyosa + state.kumi_info[13][h] > handvalue)
						{ //&&nagashi_kumi2+1>=nagashi_kumi&&kumisuu2-nagashi_kumi2<=kumisuu-nagashi_kumi

							handvalue = tsuyosa + state.kumi_info[13][h];
							copyTable(out_cards, temp2);
							state.tsuyosa = tsuyosa;

							//nagashi_kumi=nagashi_kumi2+nagashi_flag;
						}
					}
					/*
				if(shibari_flag==1&&tsuyosa+state.kumi_info[13][h]>=handvalue&&nagashi_kumi2+nagashi_flag>=nagashi_kumi&&y>=x&&flag_sh==0&&kumi_rank_s>=kumi_rank-5){//&&kumi_rank_s>=kumi_rank-5
						if(handvalue==tsuyosa+state.kumi_info[13][h]){
							handvalue=tsuyosa+state.kumi_info[13][h]+state.qty-1;
						}else{
							handvalue=tsuyosa+state.kumi_info[13][h];
						}
						copyTable(out_cards,temp2);
						state.tsuyosa=tsuyosa;
						x=y;
						kumi_rank_s=kumi_rank;
					
						flag_sh=1;
						
						printf("game_count   %d\n",state.game_count);
						printf("temp2\n");
						outputCards(temp2);

				}
				*/
				}
			}
		}
	}
}

void my_followSequence3(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int group[8][15];
	int ngroup[8][15];

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};
	int copy_cards[8][15] = {{0}};

	int tsuyosa = 0, ba_suit = 0, kumi_suit = 0, j = 0;
	int handvalue = 0;

	int h = 4;
	int rank2 = 0, pattern = 0;
	int nagashi_kumi = 0, nagashi_kumi2 = 0, nagashi_flag = 0;
	int kumisuu = 0, kumisuu2 = 0;
	int tsuyosa2 = 0;
	int hh = 15, x = 0, y = 0;

	int kumi_rank = 0;
	int kumi_rank_s = 0;
	int shibari_flag = 0;

	int flag_sh = 0;

	int rest_rank_max = 0;
	int flag = 0;

	int mark = 0;

	rest_rank_max = get_rank_max(state.rest_cards);

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);
	if (state.player_number > 2)
	{
		handvalue = state.kumi_info[13][h] - 1;
	}
	else
	{
		handvalue = 0;
	}
	nagashi_kumi = state.kumi_info[13][35];
	kumisuu = state.kumi_info[13][0];

	copyTable(copy_cards, my_cards);

	if (state.suit[0] == 1)
		ba_suit = 0;
	if (state.suit[1] == 1)
		ba_suit = 1;
	if (state.suit[2] == 1)
		ba_suit = 2;
	if (state.suit[3] == 1)
		ba_suit = 3;

	highCards(temp, copy_cards, state.ord); //場より強いカードを残す
	if (state.lock == 1)
	{								 //ロックされているとき
		lockCards(temp, state.suit); //出せないカードを除去
	}

	if (qtyOfCards(temp) >= state.qty)
	{

		for (pattern = 0; pattern < 12; pattern++)
		{
			for (mark = 0; mark < 4; mark++)
			{

				copyCards(copy_cards, my_cards);

				Sequence2(temp2, temp, mark, state.qty, pattern);

				if (qtyOfCards(temp2) == state.qty && get_rank_min(temp2) > state.ord)
				{

					kumi_rank = 0;
					shibari_flag = 0;
					flag = 0;

					kumi_suit = get_suit(temp2);
					kumi_rank = get_rank_min(temp2);
					if (kumi_suit == ba_suit)
					{
						tsuyosa = setvalue_kaidan_shibari3(temp2, kumi_suit);
						tsuyosa2 = setvalue_kaidan3(temp2);
						if (state.player_number > 2)
						{
							/*
							if(tsuyosa<STRONG*state.qty&&tsuyosa2!=tsuyosa&&flag==0){
								/tsuyosa=tsuyosa2+state.qty-1;
								flag=1;
							}
							*/

							if (tsuyosa == STRONG * state.qty)
							{
								if (tsuyosa2 < tsuyosa)
								{
									//tsuyosa=tsuyosa2+2;
									tsuyosa = tsuyosa - 1;
								}
								else
								{
									tsuyosa = tsuyosa2;
								}
							}
							else
							{
								tsuyosa = tsuyosa2;
							}

							shibari_flag = 1;
						}
						else
						{
							if (tsuyosa < STRONG * state.qty && tsuyosa2 != tsuyosa)
								tsuyosa = tsuyosa2;
						}
						if (tsuyosa > STRONG * state.qty)
							tsuyosa = STRONG * state.qty;
					}
					else
					{
						tsuyosa = setvalue_kaidan3(temp2);
					}

					cardsDiff3(copy_cards, temp2);
					bunkatsu_pre2(copy_cards);

					if (tsuyosa >= (STRONG - 1) * state.qty)
					{
						nagashi_flag = 1;
					}
					else
					{
						nagashi_flag = 0;
					}
					nagashi_kumi2 = state.kumi_info[13][35];
					kumisuu2 = state.kumi_info[13][0];

					//y=(state.kumi_info[13][hh]+(tsuyosa/state.qty))/(state.kumi_info[13][0]+1);

					if (state.player_number > 2)
					{
						/*
					if(tsuyosa+state.kumi_info[13][h]==handvalue&&y>=x&&nagashi_kumi2+nagashi_flag>=nagashi_kumi&&kumi_rank==kumi_rank_s&&kumi_suit==ba_suit){//&&nagashi_kumi2+1>=nagashi_kumi&&kumisuu2-nagashi_kumi2<=kumisuu-nagashi_kumi
						//&&nagashi_kumi2+nagashi_flag>=nagashi_kumi
						handvalue=tsuyosa+state.kumi_info[13][h];
						copyTable(out_cards,temp2);
						state.tsuyosa=tsuyosa;
						x=y;
						kumi_rank_s=kumi_rank;
					}
					*/
						//&&y>=x
						if (tsuyosa + state.kumi_info[13][h] > handvalue && nagashi_kumi2 + nagashi_flag >= nagashi_kumi)
						{ //&&nagashi_kumi2+1>=nagashi_kumi&&kumisuu2-nagashi_kumi2<=kumisuu-nagashi_kumi
							//&&nagashi_kumi2+nagashi_flag>=nagashi_kumi
							handvalue = tsuyosa + state.kumi_info[13][h];
							copyTable(out_cards, temp2);
							state.tsuyosa = tsuyosa;
							x = y;
							kumi_rank_s = kumi_rank;
						}
					}
					else
					{
						if (tsuyosa + state.kumi_info[13][h] > handvalue)
						{ //&&nagashi_kumi2+1>=nagashi_kumi&&kumisuu2-nagashi_kumi2<=kumisuu-nagashi_kumi

							handvalue = tsuyosa + state.kumi_info[13][h];
							copyTable(out_cards, temp2);
							state.tsuyosa = tsuyosa;
						}
					}
				}
			}
		}
	}
}
/*
void my_followSequence3(int out_cards[8][15],int my_cards[8][15],int joker_flag){

  int seq[8][15];
  int nseq[8][15];
 
  int temp[8][15]={{0}}; 
  int temp2[8][15]={{0}};
  int copy_cards[8][15]={{0}};
  int copy_cards2[8][15]={{0}};
  int copy_cards3[8][15]={{0}};
  int copy_cards4[8][15]={{0}};
	
  int tsuyosa=0,ba_suit=0,kumi_suit=0,j=0;
  int handvalue=0;
	
  int h=4;
  int nagashi_kumi=0,nagashi_kumi2=0,nagashi_flag=0;
	
  clearTable(out_cards);
  bunkatsu_pre2(my_cards);
  if(state.player_number>2){
		handvalue=state.kumi_info[13][h]-1;
	}else{
		//handvalue=state.kumi_info[13][h]-10;
		handvalue=0;
  }
  nagashi_kumi=state.kumi_info[13][35];

  copyTable(copy_cards,my_cards);
  copyTable(copy_cards2,my_cards);
  copyTable(copy_cards3,my_cards);
  copyTable(copy_cards4,my_cards);
	
  if(state.suit[0]==1)ba_suit=0;
  if(state.suit[1]==1)ba_suit=1;
  if(state.suit[2]==1)ba_suit=2;
  if(state.suit[3]==1)ba_suit=3;
	
  highCards(temp,my_cards,state.ord);          //場より強いカードを残す
  if(state.lock==1){                           //ロックされているとき
    lockCards(temp,state.suit);                //出せないカードを除去
  }
  makeKaidanTable(seq,temp);                   //階段を書き出す
  nCards2(nseq,seq,state.qty);
  
  		lowSequence(temp2,copy_cards,nseq);//弱いほうから1組
		 if(qtyOfCards(temp2)==state.qty){
		 	kumi_suit=get_suit(temp2);
			if(kumi_suit==ba_suit){
				tsuyosa=setvalue_kaidan_shibari3(temp2,kumi_suit);
				if(tsuyosa<STRONG*state.qty)tsuyosa=setvalue_kaidan3(temp2);
			}else{
				tsuyosa=setvalue_kaidan3(temp2);
			}
		 	
			cardsDiff3(copy_cards,temp2);
		 	bunkatsu_pre2(copy_cards);
		 	if(tsuyosa>=(STRONG-1)*state.qty){
					nagashi_flag=1;
				}else{
					nagashi_flag=0;
				}
		 	nagashi_kumi2=state.kumi_info[13][35];
			
		 	if(state.player_number>2){
			 	if(tsuyosa+state.kumi_info[13][h]>handvalue&&nagashi_kumi2+nagashi_flag>=nagashi_kumi){//&&nagashi_kumi2+1>=nagashi_kumi
			 	
						handvalue=tsuyosa+state.kumi_info[13][h];
						copyTable(out_cards,temp2);
			 			state.tsuyosa=tsuyosa;
			
				}
		 	}else{
		 		if(tsuyosa+state.kumi_info[13][h]>handvalue){//&&nagashi_kumi2+1>=nagashi_kumi
			 		
						handvalue=tsuyosa+state.kumi_info[13][h];
						copyTable(out_cards,temp2);
			 			state.tsuyosa=tsuyosa;
			 		
				}
		 	}
		 		
		 }
	
		clearTable(temp2);
		highSequence(temp2,copy_cards2,nseq);//強いほうから1組
		if(qtyOfCards(temp2)==state.qty){
		 	kumi_suit=get_suit(temp2);
			if(kumi_suit==ba_suit){
				tsuyosa=setvalue_kaidan_shibari3(temp2,kumi_suit);
				if(tsuyosa<STRONG*state.qty)tsuyosa=setvalue_kaidan3(temp2);
			}else{
				tsuyosa=setvalue_kaidan3(temp2);
			}
		 	
			cardsDiff3(copy_cards2,temp2);
		 	bunkatsu_pre2(copy_cards2);
			if(tsuyosa>=(STRONG-1)*state.qty){
					nagashi_flag=1;
				}else{
					nagashi_flag=0;
				}
			nagashi_kumi2=state.kumi_info[13][35];
			
		 	if(state.player_number>2){
			 	if(tsuyosa+state.kumi_info[13][h]>handvalue&&nagashi_kumi2+nagashi_flag>=nagashi_kumi){//&&nagashi_kumi2+1>=nagashi_kumi
			 	
						handvalue=tsuyosa+state.kumi_info[13][h];
						copyTable(out_cards,temp2);
			 			state.tsuyosa=tsuyosa;
			
				}
		 	}else{
		 		if(tsuyosa+state.kumi_info[13][h]>handvalue){//&&nagashi_kumi2+1>=nagashi_kumi
			 		
						handvalue=tsuyosa+state.kumi_info[13][h];
						copyTable(out_cards,temp2);
			 			state.tsuyosa=tsuyosa;
			 		
				}
		 	}
		}
	
		if(state.joker==1){
    		//ジョーカーを使って探す
	   	 	makeJKaidanTable(seq,temp);
	   		nCards2(nseq,seq,state.qty); 
			
			clearTable(temp2);
			lowSequence(temp2,copy_cards3,nseq);//弱いほうから1組
			 if(qtyOfCards(temp2)==state.qty){
			 	kumi_suit=get_suit(temp2);
				if(kumi_suit==ba_suit){
					tsuyosa=setvalue_kaidan_shibari3(temp2,kumi_suit);
					if(tsuyosa<STRONG*state.qty)tsuyosa=setvalue_kaidan3(temp2);
				}else{
					tsuyosa=setvalue_kaidan3(temp2);
				}
			 	
				cardsDiff3(copy_cards3,temp2);
			 	bunkatsu_pre2(copy_cards3);
			 	if(tsuyosa>=(STRONG-1)*state.qty){
					nagashi_flag=1;
				}else{
					nagashi_flag=0;
				}
			 	nagashi_kumi2=state.kumi_info[13][35];
				
			 	if(state.player_number>2){
					if(tsuyosa+state.kumi_info[13][h]>handvalue&&nagashi_kumi2+nagashi_flag>=nagashi_kumi){//&&nagashi_kumi2+1>=nagashi_kumi
					 	
							handvalue=tsuyosa+state.kumi_info[13][h];
							copyTable(out_cards,temp2);
					 		state.tsuyosa=tsuyosa;
					}
				
				}else{
				 	if(tsuyosa+state.kumi_info[13][h]>handvalue){//&&nagashi_kumi2+1>=nagashi_kumi
					 		
							handvalue=tsuyosa+state.kumi_info[13][h];
							copyTable(out_cards,temp2);
					 		state.tsuyosa=tsuyosa;
					 		
					}
				}
		 	}
	
			clearTable(temp2);
			highSequence(temp2,copy_cards4,nseq);//強いほうから1組
			if(qtyOfCards(temp2)==state.qty){
			 	kumi_suit=get_suit(temp2);
				if(kumi_suit==ba_suit){
					tsuyosa=setvalue_kaidan_shibari3(temp2,kumi_suit);
					if(tsuyosa<STRONG*state.qty)tsuyosa=setvalue_kaidan3(temp2);
				}else{
					tsuyosa=setvalue_kaidan3(temp2);
				}
			 	
				cardsDiff3(copy_cards4,temp2);
			 	bunkatsu_pre2(copy_cards4);
				if(tsuyosa>=(STRONG-1)*state.qty){
					nagashi_flag=1;
				}else{
					nagashi_flag=0;
				}
				nagashi_kumi2=state.kumi_info[13][35];
				
			 	if(state.player_number>2){
					if(tsuyosa+state.kumi_info[13][h]>handvalue&&nagashi_kumi2+nagashi_flag>=nagashi_kumi){//&&nagashi_kumi2+1>=nagashi_kumi
					 	
							handvalue=tsuyosa+state.kumi_info[13][h];
							copyTable(out_cards,temp2);
					 		state.tsuyosa=tsuyosa;
					}
				
				}else{
				 	if(tsuyosa+state.kumi_info[13][h]>handvalue){//&&nagashi_kumi2+1>=nagashi_kumi
					 		
							handvalue=tsuyosa+state.kumi_info[13][h];
							copyTable(out_cards,temp2);
					 		state.tsuyosa=tsuyosa;
					 		
					}
				}
			}
				
		}

}
*/

void my_lead5(int out_cards[8][15], int my_cards[8][15])
{

	//革命を起こすことができるか、起こしてよいのか、を判断する

	int kuminumber = 20;
	int copy_cards[8][15] = {{0}};
	int copy_cards2[8][15] = {{0}};
	int copy_cards3[8][15] = {{0}};
	int temp[8][15] = {{0}};
	int hako1 = 0, hako2 = 0;
	int i = 0;
	int hako3 = 0;
	int hako4 = 0;
	int flag2 = 0;
	int kumi_rev_rank = 20;
	int nokorimono3 = 0;
	int nokorimono4 = 0;

	copyCards(copy_cards, my_cards);

	state.cause_rev = 1;
	bunkatsu_pre2(copy_cards);
	state.cause_rev = 0;

	//clearTable(out_cards);

	if (state.kumi_info[13][1] != 0)
	{
		//printf("kakumei\n");
		hako1 = state.kumi_info[13][17]; //現時点の手札に得点付け
										 /*
		printf("copy_cards\n");
		outputCards(copy_cards);
		*/
		kuminumber = max_kumi_info(14);	 //革命を起こす組を見つける
		kumi_rev_rank = (state.kumi_info[kuminumber][3] + state.kumi_info[kuminumber][4]) / 2;
		//printf("state.kumi_info[3]   %d\n",state.kumi_info[kuminumber][3]);
		//printf("state.kumi_info[4]   %d\n",state.kumi_info[kuminumber][4]);

		//革命を起こす組を一旦手札から取り除く
		if (kuminumber == 0)
			cardsDiff3(copy_cards, state.kumi0);
		else if (kuminumber == 1)
			cardsDiff3(copy_cards, state.kumi1);
		else if (kuminumber == 2)
			cardsDiff3(copy_cards, state.kumi2);
		else if (kuminumber == 3)
			cardsDiff3(copy_cards, state.kumi3);
		else if (kuminumber == 4)
			cardsDiff3(copy_cards, state.kumi4);
		else if (kuminumber == 5)
			cardsDiff3(copy_cards, state.kumi5);
		else if (kuminumber == 6)
			cardsDiff3(copy_cards, state.kumi6);
		else if (kuminumber == 7)
			cardsDiff3(copy_cards, state.kumi7);
		else if (kuminumber == 8)
			cardsDiff3(copy_cards, state.kumi8);
		else if (kuminumber == 9)
			cardsDiff3(copy_cards, state.kumi9);
		else if (kuminumber == 10)
			cardsDiff3(copy_cards, state.kumi10);

		if (kuminumber == 0)
			copyCards(copy_cards3, state.kumi0);
		else if (kuminumber == 1)
			copyCards(copy_cards3, state.kumi1);
		else if (kuminumber == 2)
			copyCards(copy_cards3, state.kumi2);
		else if (kuminumber == 3)
			copyCards(copy_cards3, state.kumi3);
		else if (kuminumber == 4)
			copyCards(copy_cards3, state.kumi4);
		else if (kuminumber == 5)
			copyCards(copy_cards3, state.kumi5);
		else if (kuminumber == 6)
			copyCards(copy_cards3, state.kumi6);
		else if (kuminumber == 7)
			copyCards(copy_cards3, state.kumi7);
		else if (kuminumber == 8)
			copyCards(copy_cards3, state.kumi8);
		else if (kuminumber == 9)
			copyCards(copy_cards3, state.kumi9);
		else if (kuminumber == 10)
			copyCards(copy_cards3, state.kumi10);

		//if(state.kumi_info[kuminumber][3]>=12)flag2=1;

		/*
		printf("copy_cards3\n");
		outputCards(copy_cards3);
		*/
		bunkatsu_pre2(copy_cards);
		hako4 = state.kumi_info[13][17];
		nokorimono4 = state.kumi_info[13][0] - state.kumi_info[13][6];

		//その後、場を流せる強いカードの組を見つけ、一旦取り除き、そのうえで、「「革命時の」1枚当たりの強さ評価値の合計」を組数で割った値を求める。
		state.rev = 1;
		bunkatsu_pre2(copy_cards);
		state.rev = 0;
		hako3 = state.kumi_info[13][18];
		nokorimono3 = state.kumi_info[13][0] - state.kumi_info[13][7];
		while (1)
		{
			kuminumber = max_kumi_info(3);
			if (state.kumi_info[kuminumber][10] >= STRONG)
			{ //||(state.kumi_info[kuminumber][6]>=STRONG-1&&state.kumi_info[kuminumber][1]>=2&&state.kumi_info[kuminumber][3]==12&&qtyOfCards5(state.rest_cards,13)<=3)
				state.kumi_info[kuminumber][1] = 0;
				if (state.kumi_info[kuminumber][3] != 6 && state.kumi_info[kuminumber][4] != 6)
				{

					if (kuminumber == 0)
						cardsDiff3(copy_cards, state.kumi0);
					else if (kuminumber == 1)
						cardsDiff3(copy_cards, state.kumi1);
					else if (kuminumber == 2)
						cardsDiff3(copy_cards, state.kumi2);
					else if (kuminumber == 3)
						cardsDiff3(copy_cards, state.kumi3);
					else if (kuminumber == 4)
						cardsDiff3(copy_cards, state.kumi4);
					else if (kuminumber == 5)
						cardsDiff3(copy_cards, state.kumi5);
					else if (kuminumber == 6)
						cardsDiff3(copy_cards, state.kumi6);
					else if (kuminumber == 7)
						cardsDiff3(copy_cards, state.kumi7);
					else if (kuminumber == 8)
						cardsDiff3(copy_cards, state.kumi8);
					else if (kuminumber == 9)
						cardsDiff3(copy_cards, state.kumi9);
					else if (kuminumber == 10)
						cardsDiff3(copy_cards, state.kumi10);

					if (kuminumber == 0)
						copyCards(copy_cards2, state.kumi0);
					else if (kuminumber == 1)
						copyCards(copy_cards2, state.kumi1);
					else if (kuminumber == 2)
						copyCards(copy_cards2, state.kumi2);
					else if (kuminumber == 3)
						copyCards(copy_cards2, state.kumi3);
					else if (kuminumber == 4)
						copyCards(copy_cards2, state.kumi4);
					else if (kuminumber == 5)
						copyCards(copy_cards2, state.kumi5);
					else if (kuminumber == 6)
						copyCards(copy_cards2, state.kumi6);
					else if (kuminumber == 7)
						copyCards(copy_cards2, state.kumi7);
					else if (kuminumber == 8)
						copyCards(copy_cards2, state.kumi8);
					else if (kuminumber == 9)
						copyCards(copy_cards2, state.kumi9);
					else if (kuminumber == 10)
						copyCards(copy_cards2, state.kumi10);
				}
			}
			else
			{

				break;
			}
		}

		state.rev = 1;
		bunkatsu_pre2(copy_cards);
		state.rev = 0;
		/*
		printf("copy_cards\n");
		outputCards(copy_cards);
		*/

		//何もしなかったときの手札の点数と、手札の流せる札を消費した後の革命後の手札の点数を比較して
		/*
		printf("hako1                %d\n",hako1);
		printf("hako2                %d\n",hako2);
		printf("hako3                %d\n",hako3);
		printf("hako4                %d\n",hako4);
		*/
		/*
		printf("copy_cards3\n");
		outputCards(copy_cards3);
		printf("copy_cards2\n");
		outputCards(copy_cards2);
		*/
		//後のほうがよければ、もしくは後の手札の組数が１になるなら、消費した後に、革命を起こす。
		if ((hako4 <= hako3 + 1) && kumi_rev_rank < 13)
		{ //||nokorimono3<nokorimono4
			copyTable(out_cards, copy_cards2);
			if (beEmptyCards(out_cards) == 1)
				copyTable(out_cards, copy_cards3);
		}

		if (flag2 == 1)
			clearTable(out_cards);

		if (state.kumi_info[13][0] - state.kumi_info[13][7] <= 1)
		{
			copyTable(out_cards, copy_cards2);
			if (beEmptyCards(out_cards) == 1)
				copyTable(out_cards, copy_cards3);
		}
	}
}

void my_lead5_1(int out_cards[8][15], int my_cards[8][15])
{ //革命を用いた勝ち手札の確認（のみ）

	//革命を起こすことができるか、起こしてよいのか、を判断する

	int kuminumber = 20;
	int copy_cards[8][15] = {{0}};
	int copy_cards2[8][15] = {{0}};
	int copy_cards3[8][15] = {{0}};
	int temp[8][15] = {{0}};
	int hako1 = 0, hako2 = 0;
	int i = 0;
	int hako3 = 0;
	int hako4 = 0;
	int flag2 = 0;
	int kumi_rev_rank = 20;
	int nokorimono3 = 0;
	int nokorimono4 = 0;

	copyCards(copy_cards, my_cards);

	state.cause_rev = 1;
	bunkatsu_pre2(copy_cards);
	state.cause_rev = 0;

	//clearTable(out_cards);

	if (state.kumi_info[13][1] != 0)
	{
		//printf("kakumei\n");
		hako1 = state.kumi_info[13][17]; //現時点の手札に得点付け
										 /*
		printf("copy_cards\n");
		outputCards(copy_cards);
		*/
		kuminumber = max_kumi_info(14);	 //革命を起こす組を見つける
		kumi_rev_rank = (state.kumi_info[kuminumber][3] + state.kumi_info[kuminumber][4]) / 2;
		//printf("state.kumi_info[3]   %d\n",state.kumi_info[kuminumber][3]);
		//printf("state.kumi_info[4]   %d\n",state.kumi_info[kuminumber][4]);

		//革命を起こす組を一旦手札から取り除く
		if (kuminumber == 0)
			cardsDiff3(copy_cards, state.kumi0);
		else if (kuminumber == 1)
			cardsDiff3(copy_cards, state.kumi1);
		else if (kuminumber == 2)
			cardsDiff3(copy_cards, state.kumi2);
		else if (kuminumber == 3)
			cardsDiff3(copy_cards, state.kumi3);
		else if (kuminumber == 4)
			cardsDiff3(copy_cards, state.kumi4);
		else if (kuminumber == 5)
			cardsDiff3(copy_cards, state.kumi5);
		else if (kuminumber == 6)
			cardsDiff3(copy_cards, state.kumi6);
		else if (kuminumber == 7)
			cardsDiff3(copy_cards, state.kumi7);
		else if (kuminumber == 8)
			cardsDiff3(copy_cards, state.kumi8);
		else if (kuminumber == 9)
			cardsDiff3(copy_cards, state.kumi9);
		else if (kuminumber == 10)
			cardsDiff3(copy_cards, state.kumi10);

		if (kuminumber == 0)
			copyCards(copy_cards3, state.kumi0);
		else if (kuminumber == 1)
			copyCards(copy_cards3, state.kumi1);
		else if (kuminumber == 2)
			copyCards(copy_cards3, state.kumi2);
		else if (kuminumber == 3)
			copyCards(copy_cards3, state.kumi3);
		else if (kuminumber == 4)
			copyCards(copy_cards3, state.kumi4);
		else if (kuminumber == 5)
			copyCards(copy_cards3, state.kumi5);
		else if (kuminumber == 6)
			copyCards(copy_cards3, state.kumi6);
		else if (kuminumber == 7)
			copyCards(copy_cards3, state.kumi7);
		else if (kuminumber == 8)
			copyCards(copy_cards3, state.kumi8);
		else if (kuminumber == 9)
			copyCards(copy_cards3, state.kumi9);
		else if (kuminumber == 10)
			copyCards(copy_cards3, state.kumi10);

		//if(state.kumi_info[kuminumber][3]>=12)flag2=1;

		/*
		printf("copy_cards3\n");
		outputCards(copy_cards3);
		*/
		bunkatsu_pre2(copy_cards);
		hako4 = state.kumi_info[13][17];
		nokorimono4 = state.kumi_info[13][0] - state.kumi_info[13][6];

		//その後、場を流せる強いカードの組を見つけ、一旦取り除き、そのうえで、「「革命時の」1枚当たりの強さ評価値の合計」を組数で割った値を求める。
		state.rev = 1;
		bunkatsu_pre2(copy_cards);
		state.rev = 0;
		hako3 = state.kumi_info[13][18];
		nokorimono3 = state.kumi_info[13][0] - state.kumi_info[13][7];
		while (1)
		{
			kuminumber = max_kumi_info(3);
			if (state.kumi_info[kuminumber][10] >= STRONG)
			{ //||(state.kumi_info[kuminumber][6]>=STRONG-1&&state.kumi_info[kuminumber][1]>=2&&state.kumi_info[kuminumber][3]==12&&qtyOfCards5(state.rest_cards,13)<=3)
				state.kumi_info[kuminumber][1] = 0;
				if (state.kumi_info[kuminumber][3] != 6 && state.kumi_info[kuminumber][4] != 6)
				{

					if (kuminumber == 0)
						cardsDiff3(copy_cards, state.kumi0);
					else if (kuminumber == 1)
						cardsDiff3(copy_cards, state.kumi1);
					else if (kuminumber == 2)
						cardsDiff3(copy_cards, state.kumi2);
					else if (kuminumber == 3)
						cardsDiff3(copy_cards, state.kumi3);
					else if (kuminumber == 4)
						cardsDiff3(copy_cards, state.kumi4);
					else if (kuminumber == 5)
						cardsDiff3(copy_cards, state.kumi5);
					else if (kuminumber == 6)
						cardsDiff3(copy_cards, state.kumi6);
					else if (kuminumber == 7)
						cardsDiff3(copy_cards, state.kumi7);
					else if (kuminumber == 8)
						cardsDiff3(copy_cards, state.kumi8);
					else if (kuminumber == 9)
						cardsDiff3(copy_cards, state.kumi9);
					else if (kuminumber == 10)
						cardsDiff3(copy_cards, state.kumi10);

					if (kuminumber == 0)
						copyCards(copy_cards2, state.kumi0);
					else if (kuminumber == 1)
						copyCards(copy_cards2, state.kumi1);
					else if (kuminumber == 2)
						copyCards(copy_cards2, state.kumi2);
					else if (kuminumber == 3)
						copyCards(copy_cards2, state.kumi3);
					else if (kuminumber == 4)
						copyCards(copy_cards2, state.kumi4);
					else if (kuminumber == 5)
						copyCards(copy_cards2, state.kumi5);
					else if (kuminumber == 6)
						copyCards(copy_cards2, state.kumi6);
					else if (kuminumber == 7)
						copyCards(copy_cards2, state.kumi7);
					else if (kuminumber == 8)
						copyCards(copy_cards2, state.kumi8);
					else if (kuminumber == 9)
						copyCards(copy_cards2, state.kumi9);
					else if (kuminumber == 10)
						copyCards(copy_cards2, state.kumi10);
				}
			}
			else
			{

				break;
			}
		}

		state.rev = 1;
		bunkatsu_pre2(copy_cards);
		state.rev = 0;
		/*
		printf("copy_cards\n");
		outputCards(copy_cards);
		*/

		//何もしなかったときの手札の点数と、手札の流せる札を消費した後の革命後の手札の点数を比較して
		/*
		printf("hako1                %d\n",hako1);
		printf("hako2                %d\n",hako2);
		printf("hako3                %d\n",hako3);
		printf("hako4                %d\n",hako4);
		*/
		/*
		printf("copy_cards3\n");
		outputCards(copy_cards3);
		printf("copy_cards2\n");
		outputCards(copy_cards2);
		*/
		//後のほうがよければ、もしくは後の手札の組数が１になるなら、消費した後に、革命を起こす。

		/*
		if((hako4<=hako3+1)&&kumi_rev_rank<13){//||nokorimono3<nokorimono4
			copyTable(out_cards,copy_cards2);
			if(beEmptyCards(out_cards)==1)copyTable(out_cards,copy_cards3);	
		}
		
		if(flag2==1)clearTable(out_cards);
		*/

		if (state.kumi_info[13][0] - state.kumi_info[13][7] <= 1)
		{
			copyTable(out_cards, copy_cards2);
			if (beEmptyCards(out_cards) == 1)
				copyTable(out_cards, copy_cards3);
		}
	}
}

int my_lead5_4(int out_cards[8][15], int my_cards[8][15])
{

	//革命を起こすことができるか、起こしてよいのか、を判断する

	int kuminumber = 20;
	int copy_cards[8][15] = {{0}};
	int copy_cards2[8][15] = {{0}};
	int copy_cards3[8][15] = {{0}};
	int temp[8][15] = {{0}};
	int hako1 = 0, hako2 = 0;
	int i = 0;
	int hako3 = 0;
	int hako4 = 0;
	int flag2 = 0;
	int kumi_rev_rank = 20;
	int nokorimono3 = 0;
	int nokorimono4 = 0;

	copyCards(copy_cards, my_cards);
	/*
	printf("start\n");
	outputCards(copy_cards);
	*/

	state.cause_rev = 1;
	bunkatsu_pre2(copy_cards);
	state.cause_rev = 0;
	/*
	printf("start 2\n");
	outputCards(copy_cards);
	*/

	//clearTable(out_cards);

	if (state.kumi_info[13][1] != 0)
	{

		hako1 = state.kumi_info[13][17]; //現時点の手札に得点付け

		kuminumber = max_kumi_info(14); //革命を起こす組を見つける
		kumi_rev_rank = (state.kumi_info[kuminumber][3] + state.kumi_info[kuminumber][4]) / 2;

		//革命を起こす組を一旦手札から取り除く
		if (kuminumber == 0)
			cardsDiff3(copy_cards, state.kumi0);
		else if (kuminumber == 1)
			cardsDiff3(copy_cards, state.kumi1);
		else if (kuminumber == 2)
			cardsDiff3(copy_cards, state.kumi2);
		else if (kuminumber == 3)
			cardsDiff3(copy_cards, state.kumi3);
		else if (kuminumber == 4)
			cardsDiff3(copy_cards, state.kumi4);
		else if (kuminumber == 5)
			cardsDiff3(copy_cards, state.kumi5);
		else if (kuminumber == 6)
			cardsDiff3(copy_cards, state.kumi6);
		else if (kuminumber == 7)
			cardsDiff3(copy_cards, state.kumi7);
		else if (kuminumber == 8)
			cardsDiff3(copy_cards, state.kumi8);
		else if (kuminumber == 9)
			cardsDiff3(copy_cards, state.kumi9);
		else if (kuminumber == 10)
			cardsDiff3(copy_cards, state.kumi10);

		if (kuminumber == 0)
			copyCards(copy_cards3, state.kumi0);
		else if (kuminumber == 1)
			copyCards(copy_cards3, state.kumi1);
		else if (kuminumber == 2)
			copyCards(copy_cards3, state.kumi2);
		else if (kuminumber == 3)
			copyCards(copy_cards3, state.kumi3);
		else if (kuminumber == 4)
			copyCards(copy_cards3, state.kumi4);
		else if (kuminumber == 5)
			copyCards(copy_cards3, state.kumi5);
		else if (kuminumber == 6)
			copyCards(copy_cards3, state.kumi6);
		else if (kuminumber == 7)
			copyCards(copy_cards3, state.kumi7);
		else if (kuminumber == 8)
			copyCards(copy_cards3, state.kumi8);
		else if (kuminumber == 9)
			copyCards(copy_cards3, state.kumi9);
		else if (kuminumber == 10)
			copyCards(copy_cards3, state.kumi10);

		bunkatsu_pre2(copy_cards);
		hako4 = state.kumi_info[13][17];
		nokorimono4 = state.kumi_info[13][0] - state.kumi_info[13][6];
		/*
		printf("start 3\n");
		outputCards(copy_cards);
		*/

		//その後、場を流せる強いカードの組を見つけ、一旦取り除き、そのうえで、「「革命時の」1枚当たりの強さ評価値の合計」を組数で割った値を求める。
		state.rev = 1;
		bunkatsu_pre2(copy_cards);
		state.rev = 0;
		hako3 = state.kumi_info[13][18];
		nokorimono3 = state.kumi_info[13][0] - state.kumi_info[13][7];
		/*
		printf("start 4\n");
		outputCards(copy_cards);
		*/

		while (1)
		{
			kuminumber = max_kumi_info(3);
			/*
			printf("kuminumber   %d\n",kuminumber);
			outputCards(my_cards);
			outputCards(copy_cards);
			outputkumi_info2(state.kumi_info);
			*/
			if (state.kumi_info[kuminumber][10] >= STRONG)
			{ //||(state.kumi_info[kuminumber][6]>=STRONG-1&&state.kumi_info[kuminumber][1]>=2&&state.kumi_info[kuminumber][3]==12&&qtyOfCards5(state.rest_cards,13)<=3)
				state.kumi_info[kuminumber][1] = 0;

				if (state.kumi_info[kuminumber][3] != 6 && state.kumi_info[kuminumber][4] != 6)
				{

					if (kuminumber == 0)
						cardsDiff3(copy_cards, state.kumi0);
					else if (kuminumber == 1)
						cardsDiff3(copy_cards, state.kumi1);
					else if (kuminumber == 2)
						cardsDiff3(copy_cards, state.kumi2);
					else if (kuminumber == 3)
						cardsDiff3(copy_cards, state.kumi3);
					else if (kuminumber == 4)
						cardsDiff3(copy_cards, state.kumi4);
					else if (kuminumber == 5)
						cardsDiff3(copy_cards, state.kumi5);
					else if (kuminumber == 6)
						cardsDiff3(copy_cards, state.kumi6);
					else if (kuminumber == 7)
						cardsDiff3(copy_cards, state.kumi7);
					else if (kuminumber == 8)
						cardsDiff3(copy_cards, state.kumi8);
					else if (kuminumber == 9)
						cardsDiff3(copy_cards, state.kumi9);
					else if (kuminumber == 10)
						cardsDiff3(copy_cards, state.kumi10);

					if (kuminumber == 0)
						copyCards(copy_cards2, state.kumi0);
					else if (kuminumber == 1)
						copyCards(copy_cards2, state.kumi1);
					else if (kuminumber == 2)
						copyCards(copy_cards2, state.kumi2);
					else if (kuminumber == 3)
						copyCards(copy_cards2, state.kumi3);
					else if (kuminumber == 4)
						copyCards(copy_cards2, state.kumi4);
					else if (kuminumber == 5)
						copyCards(copy_cards2, state.kumi5);
					else if (kuminumber == 6)
						copyCards(copy_cards2, state.kumi6);
					else if (kuminumber == 7)
						copyCards(copy_cards2, state.kumi7);
					else if (kuminumber == 8)
						copyCards(copy_cards2, state.kumi8);
					else if (kuminumber == 9)
						copyCards(copy_cards2, state.kumi9);
					else if (kuminumber == 10)
						copyCards(copy_cards2, state.kumi10);
				}
			}
			else
			{
				/*
				printf("test_while_d\n");
				outputCards(copy_cards);
				*/
				break;
			}
		}
		/*
		printf("test_while\n");
		
		state.rev=1;
		printf("test_while_a\n");
		outputCards(copy_cards);
		bunkatsu_pre2(copy_cards);
		printf("test_while_b\n");
		state.rev=0;
		
		printf("copy_cards\n");
		outputCards(copy_cards);
		*/

		//何もしなかったときの手札の点数と、手札の流せる札を消費した後の革命後の手札の点数を比較して
		/*
		printf("hako1                %d\n",hako1);
		printf("hako2                %d\n",hako2);
		printf("hako3                %d\n",hako3);
		printf("hako4                %d\n",hako4);
		*/
		/*
		printf("copy_cards3\n");
		outputCards(copy_cards3);
		printf("copy_cards2\n");
		outputCards(copy_cards2);
		*/
		//後のほうがよければ、もしくは後の手札の組数が１になるなら、消費した後に、革命を起こす。
		/*
		if((hako4<=hako3+1)&&kumi_rev_rank<13){//||nokorimono3<nokorimono4
			copyTable(out_cards,copy_cards2);
			if(beEmptyCards(out_cards)==1)copyTable(out_cards,copy_cards3);	
		}
		
		if(flag2==1)clearTable(out_cards);
		*/

		//printf("test_while2\n");
		if (state.kumi_info[13][0] - state.kumi_info[13][7] <= 1)
		{
			copyTable(out_cards, copy_cards2);
			if (beEmptyCards(out_cards) == 1)
				copyTable(out_cards, copy_cards3);
			printf("test_while3\n");
			return 1;
		}
		//printf("test_while4\n");
		return 0;
	}
	return 0;
}

void my_lead6(int out_cards[8][15], int my_cards[8][15])
{

	int kuminumber = 0;
	int copy_cards[8][15] = {{0}};
	int hokan = 20;
	int mark = 20;
	int hokan2 = 20;
	int hokan3 = 20;
	int flag = 0;

	copyCards(copy_cards, my_cards);
	bunkatsu_pre2(copy_cards);

	kuminumber = min_kumi_info_kakou(3);

	if (state.kumi_info[kuminumber][1] == 1 && state.kumi_info[kuminumber][2] == 0 && state.kumi_info[kuminumber][3] == 1 && copy_cards[4][1] == 2)
	{
		hokan2 = kuminumber;
		state.kumi_info[kuminumber][1] = 0;
		kuminumber = min_kumi_info_kakou(3);
		if (state.kumi_info[kuminumber][3] <= 2 && state.kumi_info[kuminumber][1] == 1)
		{
			printf("s3 no onzon\n");
		}
		else
		{
			kuminumber = hokan2;
		}
	}

	if (state.kumi_info[kuminumber][3] == 6 && state.kumi_info[13][0] >= 4 && state.kumi_info[kuminumber][0] != 3 && qtyOfCards7(state.rest_cards, 6) >= 2)
	{ //場に何もないときに8を出そうとしたら
		//&&qtyOfCards7(state.rest_cards,6)>=1
		state.kumi_info[kuminumber][1] = 0;
		kuminumber = min_kumi_info_kakou(3);
	}

	if (state.kumi_info[kuminumber][3] == 6 && state.kumi_info[13][0] == 3 && state.kumi_info[kuminumber][0] == 2 && qtyOfCards7(state.rest_cards, 6) >= 2)
	{ //場に何もないときに8を出そうとしたら
		//&&qtyOfCards7(state.rest_cards,6)>=1
		state.kumi_info[kuminumber][1] = 0;
		kuminumber = min_kumi_info_kakou(3);
	}

	if (state.kumi_info[kuminumber][3] == 6 && state.kumi_info[13][0] == 3 && state.kumi_info[kuminumber][0] == 1 && qtyOfCards7(state.rest_cards, 6) >= 2 && state.kumi_info[13][9] < 2)
	{ //場に何もないときに8を出そうとしたら
		//&&qtyOfCards7(state.rest_cards,6)>=1
		state.kumi_info[kuminumber][1] = 0;
		kuminumber = min_kumi_info_kakou(3);
	}
	/*
	if(state.kumi_info[kuminumber][3]==6&&state.kumi_info[13][0]==3&&state.kumi_info[kuminumber][0]!=3&&state.kumi_info[13][6]>=2){//場に何もないときに8を出そうとしたら
		state.kumi_info[kuminumber][1]=0;
		printf("8wo3kumi\n");
	}
	*/

	if (state.kumi_info[kuminumber][3] < 6 && state.kumi_info[13][0] == 3 && qtyOfCards5(copy_cards, 6) >= 1)
	{ //&&state.kumi_info[kuminumber][0]==1

		hokan = kuminumber;
		//mark=state.kumi_info[kuminumber][2];

		state.kumi_info[kuminumber][1] = 0;
		kuminumber = min_kumi_info_kakou(3);
		if (state.kumi_info[kuminumber][3] == 6 && state.kumi_info[kuminumber][0] == 1)
		{
			hokan3 = kuminumber;
			state.kumi_info[kuminumber][1] = 0;
			kuminumber = min_kumi_info_kakou(3);
			if (state.kumi_info[kuminumber][3] >= 8 && state.kumi_info[kuminumber][0] == 1)
			{
				kuminumber = hokan3; //確認の末、8単体を提出。
				printf("my_lead6 test3\n");
				flag = 1;
			}
		}
		if (flag == 0)
		{
			kuminumber = hokan;
		}
	}
	/*
	if(state.kumi_info[kuminumber][3]<6&&state.kumi_info[13][0]==3&&qtyOfCards5(copy_cards,6)>=1&&qtyOfCards7(state.rest_cards,state.kumi_info[kuminumber][3])<=0){
			hokan=kuminumber;
			//mark=state.kumi_info[kuminumber][2];
		
			state.kumi_info[kuminumber][1]=0;
			kuminumber=min_kumi_info_kakou(3);
			if(state.kumi_info[kuminumber][3]==6&&state.kumi_info[kuminumber][0]==1){
				printf("my_lead6 test4\n");
			}else{
				kuminumber=hokan;
			}
	}
	*/
	/*
	if(state.kumi_info[kuminumber][3]<4&&state.kumi_info[13][0]==3&&qtyOfCards5(copy_cards,6)>=1){
			state.kumi_info[kuminumber][1]=0;
			hokan=kuminumber;
			kuminumber=min_kumi_info_kakou(3);
			if(state.kumi_info[kuminumber][3]==6&&state.kumi_info[kuminumber][0]==1){
				printf("my_lead6 test2\n");
			}else{
				kuminumber=hokan;
			}
	}
	*/

	if (kuminumber == 0)
		copyTable(out_cards, state.kumi0);
	else if (kuminumber == 1)
		copyTable(out_cards, state.kumi1);
	else if (kuminumber == 2)
		copyTable(out_cards, state.kumi2);
	else if (kuminumber == 3)
		copyTable(out_cards, state.kumi3);
	else if (kuminumber == 4)
		copyTable(out_cards, state.kumi4);
	else if (kuminumber == 5)
		copyTable(out_cards, state.kumi5);
	else if (kuminumber == 6)
		copyTable(out_cards, state.kumi6);
	else if (kuminumber == 7)
		copyTable(out_cards, state.kumi7);
	else if (kuminumber == 8)
		copyTable(out_cards, state.kumi8);
	else if (kuminumber == 9)
		copyTable(out_cards, state.kumi9);
	else if (kuminumber == 10)
		copyTable(out_cards, state.kumi10);
}

void my_lead7(int out_cards[8][15], int my_cards[8][15])
{
	//my_lead6の8が出せるver
	int kuminumber = 0;
	int copy_cards[8][15] = {{0}};

	copyCards(copy_cards, my_cards);
	bunkatsu_pre2(copy_cards);

	kuminumber = min_kumi_info_kakou(3);

	if (kuminumber == 0)
		copyTable(out_cards, state.kumi0);
	else if (kuminumber == 1)
		copyTable(out_cards, state.kumi1);
	else if (kuminumber == 2)
		copyTable(out_cards, state.kumi2);
	else if (kuminumber == 3)
		copyTable(out_cards, state.kumi3);
	else if (kuminumber == 4)
		copyTable(out_cards, state.kumi4);
	else if (kuminumber == 5)
		copyTable(out_cards, state.kumi5);
	else if (kuminumber == 6)
		copyTable(out_cards, state.kumi6);
	else if (kuminumber == 7)
		copyTable(out_cards, state.kumi7);
	else if (kuminumber == 8)
		copyTable(out_cards, state.kumi8);
	else if (kuminumber == 9)
		copyTable(out_cards, state.kumi9);
	else if (kuminumber == 10)
		copyTable(out_cards, state.kumi10);
}

void my_lead8(int out_cards[8][15], int my_cards[8][15])
{

	int kuminumber = 0;
	int copy_cards[8][15] = {{0}};
	int maisuu = 0;

	copyCards(copy_cards, my_cards);
	bunkatsu_pre2(copy_cards);

	if (state.kumi_info[13][0] - state.kumi_info[13][6] == 2)
	{ //

		kuminumber = min_kumi_info(10);
		state.kumi_info[kuminumber][1] = 0;
		//最弱の評価値の組を見つけて、消して、

		kuminumber = min_kumi_info(10);
		maisuu = state.kumi_info[kuminumber][1];
		//2番目に弱い評価値の組を見つける。

		if (maisuu == 1 && state.kumi_info[13][24] + state.kumi_info[13][34] >= 1)
		{ //2番目に弱い組の構成枚数と同じ構成枚数かつ流し評価値が規定値を超えている組があるかを確認

			if (kuminumber == 0)
				copyTable(out_cards, state.kumi0);
			else if (kuminumber == 1)
				copyTable(out_cards, state.kumi1);
			else if (kuminumber == 2)
				copyTable(out_cards, state.kumi2);
			else if (kuminumber == 3)
				copyTable(out_cards, state.kumi3);
			else if (kuminumber == 4)
				copyTable(out_cards, state.kumi4);
			else if (kuminumber == 5)
				copyTable(out_cards, state.kumi5);
			else if (kuminumber == 6)
				copyTable(out_cards, state.kumi6);
			else if (kuminumber == 7)
				copyTable(out_cards, state.kumi7);
			else if (kuminumber == 8)
				copyTable(out_cards, state.kumi8);
			else if (kuminumber == 9)
				copyTable(out_cards, state.kumi9);
			else if (kuminumber == 10)
				copyTable(out_cards, state.kumi10);
		}

		if (maisuu == 2 && state.kumi_info[13][26] + state.kumi_info[13][34] - state.kumi_info[13][46] >= 1)
		{ //2番目に弱い組の構成枚数と同じ構成枚数かつ流し評価値が規定値を超えている組があるかを確認

			if (kuminumber == 0)
				copyTable(out_cards, state.kumi0);
			else if (kuminumber == 1)
				copyTable(out_cards, state.kumi1);
			else if (kuminumber == 2)
				copyTable(out_cards, state.kumi2);
			else if (kuminumber == 3)
				copyTable(out_cards, state.kumi3);
			else if (kuminumber == 4)
				copyTable(out_cards, state.kumi4);
			else if (kuminumber == 5)
				copyTable(out_cards, state.kumi5);
			else if (kuminumber == 6)
				copyTable(out_cards, state.kumi6);
			else if (kuminumber == 7)
				copyTable(out_cards, state.kumi7);
			else if (kuminumber == 8)
				copyTable(out_cards, state.kumi8);
			else if (kuminumber == 9)
				copyTable(out_cards, state.kumi9);
			else if (kuminumber == 10)
				copyTable(out_cards, state.kumi10);
		}

		if (maisuu == 3 && state.kumi_info[kuminumber][0] == 2 && state.kumi_info[13][28] + state.kumi_info[13][48] >= 1)
		{ //2番目に弱い組の構成枚数と同じ構成枚数かつ流し評価値が規定値を超えている組があるかを確認

			if (kuminumber == 0)
				copyTable(out_cards, state.kumi0);
			else if (kuminumber == 1)
				copyTable(out_cards, state.kumi1);
			else if (kuminumber == 2)
				copyTable(out_cards, state.kumi2);
			else if (kuminumber == 3)
				copyTable(out_cards, state.kumi3);
			else if (kuminumber == 4)
				copyTable(out_cards, state.kumi4);
			else if (kuminumber == 5)
				copyTable(out_cards, state.kumi5);
			else if (kuminumber == 6)
				copyTable(out_cards, state.kumi6);
			else if (kuminumber == 7)
				copyTable(out_cards, state.kumi7);
			else if (kuminumber == 8)
				copyTable(out_cards, state.kumi8);
			else if (kuminumber == 9)
				copyTable(out_cards, state.kumi9);
			else if (kuminumber == 10)
				copyTable(out_cards, state.kumi10);
		}
	}
}

void my_lead9(int out_cards[8][15], int my_cards[8][15])
{

	int kuminumber = 0;
	int copy_cards[8][15] = {{0}};
	int hokan = 20;
	int mark = 20;
	int hokan2 = 20;
	int hokan3 = 20;
	int flag = 0;

	copyCards(copy_cards, my_cards);
	bunkatsu_pre2(copy_cards);

	if (state.kumi_info[13][24] + state.kumi_info[13][26] + state.kumi_info[13][34] == 0)
	{ //流せる組が手札にないとき、

		kuminumber = min_kumi_info_kakou2(3); //構成枚数３枚以上の組を優先して出そうとする。

		if (state.kumi_info[kuminumber][1] == 1 && state.kumi_info[kuminumber][2] == 0 && state.kumi_info[kuminumber][3] == 1 && copy_cards[4][1] == 2)
		{
			hokan2 = kuminumber;
			state.kumi_info[kuminumber][1] = 0;
			kuminumber = min_kumi_info_kakou2(3);
			if (state.kumi_info[kuminumber][3] <= 2 && state.kumi_info[kuminumber][1] == 1)
			{
				printf("s3 no onzon\n");
			}
			else
			{
				kuminumber = hokan2;
			}
		}

		if (state.kumi_info[kuminumber][3] == 6 && state.kumi_info[13][0] >= 4 && state.kumi_info[kuminumber][0] != 3 && qtyOfCards7(state.rest_cards, 6) >= 2)
		{ //場に何もないときに8を出そうとしたら
			state.kumi_info[kuminumber][1] = 0;
			kuminumber = min_kumi_info_kakou2(3);
		}
		if (state.kumi_info[kuminumber][3] == 6 && state.kumi_info[13][0] == 3 && state.kumi_info[kuminumber][0] == 2 && qtyOfCards7(state.rest_cards, 6) >= 2)
		{ //場に何もないときに8を出そうとしたら
			state.kumi_info[kuminumber][1] = 0;
			kuminumber = min_kumi_info_kakou2(3);
		}

		if (state.kumi_info[kuminumber][3] == 6 && state.kumi_info[13][0] == 3 && state.kumi_info[kuminumber][0] == 1 && qtyOfCards7(state.rest_cards, 6) >= 2 && state.kumi_info[13][9] < 2)
		{ //場に何もないときに8を出そうとしたら
			state.kumi_info[kuminumber][1] = 0;
			kuminumber = min_kumi_info_kakou2(3);
		}
		/*
		if(state.kumi_info[kuminumber][3]==6&&state.kumi_info[13][0]==3&&state.kumi_info[kuminumber][0]!=3&&state.kumi_info[13][6]>=2){//場に何もないときに8を出そうとしたら
			state.kumi_info[kuminumber][1]=0;
			printf("8wo3kumi\n");
		}
		*/
		if (state.kumi_info[kuminumber][3] < 6 && state.kumi_info[13][0] == 3 && qtyOfCards5(copy_cards, 6) >= 1)
		{ //&&state.kumi_info[kuminumber][0]==1
			hokan = kuminumber;
			//mark=state.kumi_info[kuminumber][2];

			state.kumi_info[kuminumber][1] = 0;
			kuminumber = min_kumi_info_kakou2(3);
			if (state.kumi_info[kuminumber][3] == 6 && state.kumi_info[kuminumber][0] == 1)
			{
				hokan3 = kuminumber;
				state.kumi_info[kuminumber][1] = 0;
				kuminumber = min_kumi_info_kakou2(3);
				if (state.kumi_info[kuminumber][3] >= 8 && state.kumi_info[kuminumber][0] == 1)
				{
					kuminumber = hokan3; //確認の末、8単体を提出。
					printf("my_lead9 test3\n");
					flag = 1;
				}
			}
			if (flag == 0)
			{
				kuminumber = hokan;
			}
		}

		/*
		if(state.kumi_info[kuminumber][3]<6&&state.kumi_info[13][0]==3&&qtyOfCards5(copy_cards,6)>=1&&qtyOfCards7(state.rest_cards,state.kumi_info[kuminumber][3])<=0){
			hokan=kuminumber;
			mark=state.kumi_info[kuminumber][2];
		
			state.kumi_info[kuminumber][1]=0;
			kuminumber=min_kumi_info_kakou2(3);
			if(state.kumi_info[kuminumber][3]==6&&state.kumi_info[kuminumber][0]==1){
				printf("my_lead9 test4\n");
			}else{
				kuminumber=hokan;
			}
		}
		*/
		/*
		if(state.kumi_info[kuminumber][3]<4&&state.kumi_info[13][0]==3&&qtyOfCards5(copy_cards,6)>=1){
				state.kumi_info[kuminumber][1]=0;
				hokan=kuminumber;
				kuminumber=min_kumi_info_kakou2(3);
				if(state.kumi_info[kuminumber][3]==6&&state.kumi_info[kuminumber][0]==1){
					printf("my_lead9 test2\n");
				}else{
					kuminumber=hokan;
				}
		}
		*/

		if (kuminumber == 0)
			copyTable(out_cards, state.kumi0);
		else if (kuminumber == 1)
			copyTable(out_cards, state.kumi1);
		else if (kuminumber == 2)
			copyTable(out_cards, state.kumi2);
		else if (kuminumber == 3)
			copyTable(out_cards, state.kumi3);
		else if (kuminumber == 4)
			copyTable(out_cards, state.kumi4);
		else if (kuminumber == 5)
			copyTable(out_cards, state.kumi5);
		else if (kuminumber == 6)
			copyTable(out_cards, state.kumi6);
		else if (kuminumber == 7)
			copyTable(out_cards, state.kumi7);
		else if (kuminumber == 8)
			copyTable(out_cards, state.kumi8);
		else if (kuminumber == 9)
			copyTable(out_cards, state.kumi9);
		else if (kuminumber == 10)
			copyTable(out_cards, state.kumi10);
	}
}

void my_lead10(int out_cards[8][15], int my_cards[8][15])
{

	int kuminumber = 14;
	int copy_cards[8][15] = {{0}};
	int hand_kumi = 0, nagashi_kumi = 0, check = 0;
	int flag_eight = 0;
	int hokan = 20;
	int mark = 20;
	int hokan2 = 20;

	copyCards(copy_cards, my_cards);
	bunkatsu_pre2(copy_cards);
	hand_kumi = state.kumi_info[13][0];
	nagashi_kumi = state.kumi_info[13][35];																			//99
	check = hand_kumi - nagashi_kumi - state.kumi_info[13][36] - state.kumi_info[13][38] - state.kumi_info[13][34]; //規定値－1以上の2を含まない1枚組と2枚組の組の数と2を含む階段以外の組の数

	if (check <= 1)
	{ //||(check<=2&&hand_kumi>=10)//||(check<=2&&my_cards[0][1]==1&&my_cards[4][1]==2&&hand_kumi>=7)
		/*
		outputkumi_info(0);
		outputkumi_info(1);
		outputkumi_info(2);
		outputkumi_info(3);
		outputkumi_info(4);
		*/

		flag_eight = qtyOfCards5(my_cards, 6);

		//kuminumber=min_kumi_info(3);

		//if(state.kumi_info[kuminumber][1]>=3){
		/*
			if(kuminumber==0)copyTable(out_cards,state.kumi0);
			else if(kuminumber==1)copyTable(out_cards,state.kumi1);
			else if(kuminumber==2)copyTable(out_cards,state.kumi2);
			else if(kuminumber==3)copyTable(out_cards,state.kumi3);
			else if(kuminumber==4)copyTable(out_cards,state.kumi4);
			else if(kuminumber==5)copyTable(out_cards,state.kumi5);
			else if(kuminumber==6)copyTable(out_cards,state.kumi6);
			else if(kuminumber==7)copyTable(out_cards,state.kumi7);
			else if(kuminumber==8)copyTable(out_cards,state.kumi8);
			else if(kuminumber==9)copyTable(out_cards,state.kumi9);
			else if(kuminumber==10)copyTable(out_cards,state.kumi10);
			*/

		//}else{

		kuminumber = min_kumi_info(10);

		state.kumi_info[kuminumber][1] = 0;

		kuminumber = min_kumi_info_kakou(3);

		if (state.kumi_info[kuminumber][1] == 1 && state.kumi_info[kuminumber][2] == 0 && state.kumi_info[kuminumber][3] == 1 && copy_cards[4][1] == 2)
		{
			hokan2 = kuminumber;
			state.kumi_info[kuminumber][1] = 0;
			kuminumber = min_kumi_info_kakou(3);
			if (state.kumi_info[kuminumber][3] <= 2 && state.kumi_info[kuminumber][1] == 1)
			{
				printf("s3 no onzon\n");
			}
			else
			{
				kuminumber = hokan2;
			}
		}

		if (state.kumi_info[kuminumber][3] == 6 && state.kumi_info[13][0] >= 3 && state.kumi_info[kuminumber][0] != 3)
		{ //場に何もないときに8を出そうとしたら
			state.kumi_info[kuminumber][1] = 0;
			kuminumber = min_kumi_info_kakou(3);
		}
		/*
			if(state.kumi_info[kuminumber][3]==6&&state.kumi_info[13][0]==3&&state.kumi_info[kuminumber][0]==2&&qtyOfCards7(state.rest_cards,6)>=2){//場に何もないときに8を出そうとしたら
					state.kumi_info[kuminumber][1]=0;
					kuminumber=min_kumi_info_kakou(3);
			}
			
			if(state.kumi_info[kuminumber][3]==6&&state.kumi_info[13][0]==3&&state.kumi_info[kuminumber][0]==1&&qtyOfCards7(state.rest_cards,6)>=2&&state.kumi_info[13][9]<2){//場に何もないときに8を出そうとしたら
				state.kumi_info[kuminumber][1]=0;
				kuminumber=min_kumi_info_kakou(3);
			}
			
			if(state.kumi_info[kuminumber][3]<6&&state.kumi_info[13][0]==3&&qtyOfCards5(copy_cards,6)>=1&&state.kumi_info[kuminumber][0]==1){//&&qtyOfCards7(state.rest_cards,state.kumi_info[kuminumber][3])<=200
				hokan=kuminumber;
				//mark=state.kumi_info[kuminumber][2];
			
				state.kumi_info[kuminumber][1]=0;
				kuminumber=min_kumi_info_kakou(3);
				if(state.kumi_info[kuminumber][3]==6&&state.kumi_info[kuminumber][0]==1){
					printf("my_lead10 test3\n");
				}else{
					kuminumber=hokan;
				}
			}
			
			if(state.kumi_info[kuminumber][3]<6&&state.kumi_info[13][0]==3&&qtyOfCards5(copy_cards,6)>=1&&qtyOfCards7(state.rest_cards,state.kumi_info[kuminumber][3])<=0){//&&qtyOfCards7(state.rest_cards,state.kumi_info[kuminumber][3])<=200
				hokan=kuminumber;
				//mark=state.kumi_info[kuminumber][2];
			
				state.kumi_info[kuminumber][1]=0;
				kuminumber=min_kumi_info_kakou(3);
				if(state.kumi_info[kuminumber][3]==6&&state.kumi_info[kuminumber][0]==1){
					printf("my_lead10 test4\n");
				}else{
					kuminumber=hokan;
				}
			}
			*/

		if (kuminumber == 0)
			copyTable(out_cards, state.kumi0);
		else if (kuminumber == 1)
			copyTable(out_cards, state.kumi1);
		else if (kuminumber == 2)
			copyTable(out_cards, state.kumi2);
		else if (kuminumber == 3)
			copyTable(out_cards, state.kumi3);
		else if (kuminumber == 4)
			copyTable(out_cards, state.kumi4);
		else if (kuminumber == 5)
			copyTable(out_cards, state.kumi5);
		else if (kuminumber == 6)
			copyTable(out_cards, state.kumi6);
		else if (kuminumber == 7)
			copyTable(out_cards, state.kumi7);
		else if (kuminumber == 8)
			copyTable(out_cards, state.kumi8);
		else if (kuminumber == 9)
			copyTable(out_cards, state.kumi9);
		else if (kuminumber == 10)
			copyTable(out_cards, state.kumi10);
		/*
			if(state.kumi_info[13][0]==2&&state.kumi_info[kuminumber][0]==1)clearCards(out_cards);
			if(state.kumi_info[13][0]==2&&state.kumi_info[kuminumber][0]==2&&state.player_number<=3&&state.kumi_info[kuminumber][1]==2)clearCards(out_cards);
			if(state.kumi_info[13][0]==2&&state.kumi_info[kuminumber][0]==3&&state.player_number==2&&state.kumi_info[kuminumber][1]>=3)clearCards(out_cards);
			if(state.kumi_info[13][0]==2&&state.kumi_info[kuminumber][0]==3&&state.player_number==2)clearCards(out_cards);
			*/
		if (state.kumi_info[13][0] == 2 && state.kumi_info[kuminumber][1] == 1)
			clearCards(out_cards);
		if (state.kumi_info[13][0] == 2 && state.kumi_info[kuminumber][1] == 2 && state.player_number <= 3)
			clearCards(out_cards); //&&state.player_number<=3
		if (state.kumi_info[13][0] == 2 && state.kumi_info[kuminumber][1] >= 3 && state.player_number <= 2)
			clearCards(out_cards); //&&state.player_number<=2

		if (flag_eight >= 1 && state.kumi_info[13][0] == 3 && state.kumi_info[kuminumber][1] == 1)
			clearCards(out_cards);
		if (flag_eight >= 1 && state.kumi_info[13][0] == 3 && state.kumi_info[kuminumber][1] == 2 && state.player_number <= 2)
			clearCards(out_cards); //&&state.player_number<=3
		if (flag_eight >= 1 && state.kumi_info[13][0] == 3 && state.kumi_info[kuminumber][1] >= 3 && state.player_number <= 2)
			clearCards(out_cards); //&&state.player_number<=2
								   //}
	}
}

void my_lead11(int out_cards[8][15], int my_cards[8][15])
{

	int kuminumber = 0;
	int copy_cards[8][15] = {{0}};
	int copy_cards2[8][15] = {{0}};
	int copy_cards3[8][15] = {{0}};

	int o_cards[8][15] = {{0}};
	int o_cards2[8][15] = {{0}};

	int cards[8][15] = {{0}};
	int cards2[8][15] = {{0}};

	int tsuyosa = 0;
	int tsuyosa2 = 0;
	int tsuyosa3 = 0;

	int flag = 0;
	int max_tsuyosa = 0;
	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};
	int h = 6;

	int kumi_suit = 0;
	int kumi_suitsum = 0;
	int bunkatsu_flag = 0;

	int count = 0;
	int start_kumisuu;

	int rank = 0;
	int rank_s = 6;

	copyCards(copy_cards, my_cards);
	bunkatsu_pre2(copy_cards);
	start_kumisuu = state.kumi_info[13][0];

	while (count < start_kumisuu)
	{

		if (count == 0)
			copyTable(temp, state.kumi0);
		else if (count == 1)
			copyTable(temp, state.kumi1);
		else if (count == 2)
			copyTable(temp, state.kumi2);
		else if (count == 3)
			copyTable(temp, state.kumi3);
		else if (count == 4)
			copyTable(temp, state.kumi4);
		else if (count == 5)
			copyTable(temp, state.kumi5);
		else if (count == 6)
			copyTable(temp, state.kumi6);
		else if (count == 7)
			copyTable(temp, state.kumi7);
		else if (count == 8)
			copyTable(temp, state.kumi8);
		else if (count == 9)
			copyTable(temp, state.kumi9);
		else if (count == 10)
			copyTable(temp, state.kumi10);

		//
		if (beEmptyCards(temp) == 0)
		{

			copyTable(temp2, temp);
			copyCards(copy_cards, my_cards);
			if (bunkatsu_flag == 1)
			{
				bunkatsu_pre2(copy_cards);
				bunkatsu_flag = 0;
			}

			flag = 0; //リセット
			clearTable(o_cards);
			clearTable(o_cards2);
			clearTable(cards);
			clearTable(cards2);
			tsuyosa2 = 0;
			tsuyosa3 = 0;

			getField2(temp2);

			if (state.kumi_info[count][0] == 1)
			{
				tsuyosa = setvalue_single2(temp2);
				get_rank_min(temp2);

				cardsDiff3(copy_cards, temp2);

				copyCards(copy_cards2, copy_cards);
				copyCards(copy_cards3, copy_cards);

				follow_o_shibari(o_cards, state.rest_cards);
				follow_o2(o_cards2, state.rest_cards);

				if (qtyOfCards(o_cards) == qtyOfCards(state.rest_cards))
				{ //||qtyOfCards5(o_cards,6)>0
					flag = 99;
				}
				if (qtyOfCards(o_cards2) == qtyOfCards(state.rest_cards))
				{
					flag = 99;
				}

				if (qtyOfCards(o_cards) == 0)
				{
					flag++;
					tsuyosa2 = STRONG;
				}
				else
				{
					getField2(o_cards);
					follow_o_shibari(cards, copy_cards);
					if (qtyOfCards(cards) != 0)
					{
						flag++;
						kumi_suit = get_suit(cards);
						tsuyosa2 = setvalue_single_shibari(cards, kumi_suit);
					}
				}

				if (qtyOfCards(o_cards2) == 0)
				{
					flag++;
					tsuyosa3 = STRONG;
				}
				else
				{
					getField2(o_cards2);
					follow_o2(cards2, copy_cards);
					if (qtyOfCards(cards2) != 0)
					{
						flag++;
						tsuyosa3 = setvalue_single2(cards2);
					}
				}
			}

			if (state.kumi_info[count][0] == 2)
			{
				tsuyosa = setvalue_pair2(temp2);
				get_rank_min(temp2);

				cardsDiff3(copy_cards, temp2);

				copyCards(copy_cards2, copy_cards);
				copyCards(copy_cards3, copy_cards);

				follow_o_shibari(o_cards, state.rest_cards);
				follow_o2(o_cards2, state.rest_cards);

				if (state.player_number == 2)
				{
					if (qtyOfCards(o_cards) == qtyOfCards(state.rest_cards))
					{
						flag = 99;
					}
					if (qtyOfCards(o_cards2) == qtyOfCards(state.rest_cards))
					{
						flag = 99;
					}
				}

				if (qtyOfCards(o_cards) == 0)
				{
					flag++;
					tsuyosa2 = STRONG;
				}
				else
				{
					getField2(o_cards);
					follow_o_shibari(cards, copy_cards);
					if (qtyOfCards(cards) != 0)
					{
						flag++;
						kumi_suitsum = get_suitsum(cards);
						tsuyosa2 = setvalue_pair_shibari(cards, kumi_suitsum);
					}
				}

				if (qtyOfCards(o_cards2) == 0)
				{
					flag++;
					tsuyosa3 = STRONG;
				}
				else
				{
					getField2(o_cards2);
					follow_o2(cards2, copy_cards);
					if (qtyOfCards(cards2) != 0)
					{
						flag++;
						tsuyosa3 = setvalue_pair2(cards2);
					}
				}
			}

			if (state.kumi_info[count][0] == 3)
			{
				tsuyosa = setvalue_kaidan2(temp2);
				get_rank_min(temp2);

				cardsDiff3(copy_cards, temp2);

				copyCards(copy_cards2, copy_cards);
				copyCards(copy_cards3, copy_cards);

				follow_o_shibari(o_cards, state.rest_cards);
				follow_o2(o_cards2, state.rest_cards);

				if (state.player_number == 2)
				{
					if (qtyOfCards(o_cards) == qtyOfCards(state.rest_cards))
					{
						flag = 99;
					}
					if (qtyOfCards(o_cards2) == qtyOfCards(state.rest_cards))
					{
						flag = 99;
					}
				}

				if (qtyOfCards(o_cards) == 0)
				{
					flag++;
					tsuyosa2 = STRONG;
				}
				else
				{
					getField2(o_cards);
					follow_o_shibari(cards, copy_cards);
					if (qtyOfCards(cards) != 0)
					{
						flag++;
						kumi_suit = get_suit(cards);
						tsuyosa2 = setvalue_kaidan_shibari(cards, kumi_suit);
					}
				}

				if (qtyOfCards(o_cards2) == 0)
				{
					flag++;
					tsuyosa3 = STRONG;
				}
				else
				{
					getField2(o_cards2);
					follow_o2(cards2, copy_cards);
					if (qtyOfCards(cards2) != 0)
					{
						flag++;
						tsuyosa3 = setvalue_kaidan2(cards2);
					}
				}
			}

			if (flag == 2 && tsuyosa2 >= STRONG && tsuyosa3 >= STRONG)
			{
				cardsDiff3(copy_cards2, cards);
				bunkatsu_pre2(copy_cards2);
				bunkatsu_flag = 1;
			}

			if (flag == 2 && state.kumi_info[13][0] - state.kumi_info[13][h] <= 1 && tsuyosa2 >= STRONG && tsuyosa3 >= STRONG)
			{

				cardsDiff3(copy_cards3, cards2);
				bunkatsu_pre2(copy_cards3);

				if (state.kumi_info[13][0] - state.kumi_info[13][h] <= 1)
				{

					if (tsuyosa > max_tsuyosa)
					{
						copyTable(out_cards, temp2);
						max_tsuyosa = tsuyosa;
						/*
						outputCards(o_cards);
						outputCards(o_cards2);
						outputCards(cards);
						outputCards(cards2);
						*/
					}
					/*
					if(rank>rank_s){
						copyTable(out_cards,temp2);
						rank_s=rank;
						//max_tsuyosa=tsuyosa;
					}
					if(rank_s<=6&&rank<rank_s){
						copyTable(out_cards,temp2);
						rank_s=rank;			
					}
					*/
				}
			}
		}
		//
		count++;
	}
}

void my_lead14(int out_cards[8][15], int my_cards[8][15])
{ //提出したら負け確定の札を出さないように確認する。残り2人用。（仮）

	int kuminumber = 0;
	int copy_cards[8][15] = {{0}};
	//int copy_cards2[8][15]={{0}};
	//int copy_cards3[8][15]={{0}};

	int o_cards[8][15] = {{0}};
	int o_cards2[8][15] = {{0}};

	int cards[8][15] = {{0}};
	int cards2[8][15] = {{0}};

	int tsuyosa = 0;
	int tsuyosa2 = 0;
	int tsuyosa3 = 0;

	int flag = 0;
	int max_tsuyosa = 0;
	int min_tsuyosa = 1000;
	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};
	int h = 6;

	int kumi_suit = 0;
	int kumi_suitsum = 0;
	int bunkatsu_flag = 0;

	int count = 0;
	int start_kumisuu;

	int rank = 0;
	int rank_s = 20;

	int rest_cards2[8][15] = {{0}};
	int rest_cards3[8][15] = {{0}};
	int temp3[8][15] = {{0}};
	int copy_rest_cards[8][15] = {{0}};
	int copy_submitted_cards[8][15] = {{0}};
	int copy_submitted_cards_plus_hands[8][15] = {{0}};

	copyCards(copy_cards, my_cards);
	bunkatsu_pre2(copy_cards);
	start_kumisuu = state.kumi_info[13][0];
	copyCards(copy_rest_cards, state.rest_cards);
	copyCards(copy_submitted_cards, state.submitted_cards);
	copyCards(copy_submitted_cards_plus_hands, state.submitted_cards_plus_hands);

	while (count < start_kumisuu)
	{

		copyCards(copy_cards, my_cards);
		bunkatsu_pre2(copy_cards);

		if (count == 0)
			copyTable(temp, state.kumi0);
		else if (count == 1)
			copyTable(temp, state.kumi1);
		else if (count == 2)
			copyTable(temp, state.kumi2);
		else if (count == 3)
			copyTable(temp, state.kumi3);
		else if (count == 4)
			copyTable(temp, state.kumi4);
		else if (count == 5)
			copyTable(temp, state.kumi5);
		else if (count == 6)
			copyTable(temp, state.kumi6);
		else if (count == 7)
			copyTable(temp, state.kumi7);
		else if (count == 8)
			copyTable(temp, state.kumi8);
		else if (count == 9)
			copyTable(temp, state.kumi9);
		else if (count == 10)
			copyTable(temp, state.kumi10);

		//
		if (beEmptyCards(temp) == 0)
		{

			copyTable(temp2, temp);
			copyCards(copy_cards, my_cards);
			//if(bunkatsu_flag==1){
			//bunkatsu_pre2(copy_cards);
			//bunkatsu_flag=0;
			//}

			flag = 1; //リセット(?)
			clearTable(o_cards);
			clearTable(o_cards2);
			clearTable(cards);
			clearTable(cards2);
			tsuyosa2 = 0;
			tsuyosa3 = 0;
			copyCards(state.rest_cards, copy_rest_cards);
			copyCards(state.submitted_cards, copy_submitted_cards);
			copyCards(state.submitted_cards_plus_hands, copy_submitted_cards_plus_hands);

			getField2(temp2);

			if (state.kumi_info[count][0] == 1)
			{
				tsuyosa = setvalue_single2(temp2);
				rank = get_rank_min(temp2);
				getField2(temp2);

				cardsDiff3(copy_cards, temp2);
				flag = 0;

				//copyCards(copy_cards2,copy_cards);
				//copyCards(copy_cards3,copy_cards);
				/*
				copyCards(rest_cards2,state,rest_cards);
				copyCards(rest_cards3,state.rest_cards);
				*/
				//follow_o3_shibari(o_cards,state.rest_cards);
				follow_o3(o_cards2, state.rest_cards);
				/*
				if(qtyOfCards(o_cards)==qtyOfCards(state.rest_cards)){//||qtyOfCards5(o_cards,6)>0
					flag=99;
				}
				*/
				if (qtyOfCards(o_cards2) == qtyOfCards(state.rest_cards))
				{
					flag = 99;
				}
				/*	
				if(qtyOfCards(o_cards)==0){
					flag++;
					tsuyosa2=STRONG;
				}else{
					getField2(o_cards);
					follow_o_shibari(cards,copy_cards);
					if(qtyOfCards(cards)!=0){
						flag++;
						kumi_suit=get_suit(cards);
						tsuyosa2=setvalue_single_shibari(cards,kumi_suit);
					}
				}
						
				if(qtyOfCards(o_cards2)==0){
					flag++;
					tsuyosa3=STRONG;
				}else{
					getField2(o_cards2);
					follow_o2(cards2,copy_cards);
					if(qtyOfCards(cards2)!=0){
						flag++;
						tsuyosa3=setvalue_single2(cards2);
					}
				}
				*/
			}

			if (state.kumi_info[count][0] == 2)
			{
				tsuyosa = setvalue_pair2(temp2);
				rank = get_rank_min(temp2);
				getField2(temp2);

				cardsDiff3(copy_cards, temp2);
				flag = 0;

				//copyCards(copy_cards2,copy_cards);
				//copyCards(copy_cards3,copy_cards);
				/*
				copyCards(rest_cards2,state,rest_cards);
				copyCards(rest_cards3,state.rest_cards);
				*/
				//follow_o3_shibari(o_cards,state.rest_cards);
				follow_o3(o_cards2, state.rest_cards);

				if (state.player_number == 2)
				{
					/*
						if(qtyOfCards(o_cards)==qtyOfCards(state.rest_cards)){
							flag=99;
						}
					*/
					if (qtyOfCards(o_cards2) == qtyOfCards(state.rest_cards))
					{
						flag = 99;
					}
				}

				/*
				if(qtyOfCards(o_cards)==0){
					flag++;
					tsuyosa2=STRONG;
				}else{
					getField2(o_cards);
					follow_o_shibari(cards,copy_cards);
					if(qtyOfCards(cards)!=0){
						flag++;
						kumi_suitsum=get_suitsum(cards);
						tsuyosa2=setvalue_pair_shibari(cards,kumi_suitsum);
					}
				}
									
				if(qtyOfCards(o_cards2)==0){
					flag++;
					tsuyosa3=STRONG;
				}else{
					getField2(o_cards2);
					follow_o2(cards2,copy_cards);
					if(qtyOfCards(cards2)!=0){
						flag++;
						tsuyosa3=setvalue_pair2(cards2);
					}
				}
				*/
			}

			if (state.kumi_info[count][0] == 3)
			{
				tsuyosa = setvalue_kaidan2(temp2);
				rank = get_rank_min(temp2);
				getField2(temp2);

				cardsDiff3(copy_cards, temp2);
				flag = 0;

				//copyCards(copy_cards2,copy_cards);
				//copyCards(copy_cards3,copy_cards);
				/*
				copyCards(rest_cards2,state,rest_cards);
				copyCards(rest_cards3,state.rest_cards);
				*/
				//follow_o3_shibari(o_cards,state.rest_cards);
				follow_o3(o_cards2, state.rest_cards);

				if (state.player_number == 2)
				{
					/*
						if(qtyOfCards(o_cards)==qtyOfCards(state.rest_cards)){
							flag=99;
						}
						*/
					if (qtyOfCards(o_cards2) == qtyOfCards(state.rest_cards))
					{
						flag = 99;
					}
				}

				/*
				if(qtyOfCards(o_cards)==0){
					flag++;
					tsuyosa2=STRONG;
				}else{
					getField2(o_cards);
					follow_o_shibari(cards,copy_cards);
					if(qtyOfCards(cards)!=0){
						flag++;
						kumi_suit=get_suit(cards);
						tsuyosa2=setvalue_kaidan_shibari(cards,kumi_suit);
					}
				}
									
				if(qtyOfCards(o_cards2)==0){
					flag++;
					tsuyosa3=STRONG;
				}else{
					getField2(o_cards2);
					follow_o2(cards2,copy_cards);
					if(qtyOfCards(cards2)!=0){
						flag++;
						tsuyosa3=setvalue_kaidan2(cards2);
					}
				}
				*/
			}

			if (flag == 0)
			{
				copyCards(state.rest_cards, copy_cards);
				copyTable5(state.submitted_cards, copy_rest_cards);
				copyCards(state.submitted_cards_plus_hands, state.submitted_cards);
				my_finish_follow2_o(temp3, copy_rest_cards);
				if (qtyOfCards(temp3) != 0)
					flag = 99;
			}
			/*
			if(flag==0){
				if(tsuyosa<min_tsuyosa){
					copyTable(out_cards,temp2);
					min_tsuyosa=tsuyosa;
					
				}
			}
			*/
			if (flag == 0)
			{
				if (rank < rank_s)
				{
					copyTable(out_cards, temp2);
					rank_s = rank;
					//max_tsuyosa=tsuyosa;
				}
			}
		}
		//
		count++;
	}

	copyCards(state.rest_cards, copy_rest_cards);
	copyCards(state.submitted_cards, copy_submitted_cards);
	copyCards(state.submitted_cards_plus_hands, copy_submitted_cards_plus_hands);
}

void my_lead5_rev(int out_cards[8][15], int my_cards[8][15])
{

	//革命を起こすことができるか、起こしてよいのか、を判断する

	int kuminumber = 20;
	int copy_cards[8][15] = {{0}};
	int copy_cards2[8][15] = {{0}};
	int copy_cards3[8][15] = {{0}};
	int temp[8][15] = {{0}};
	int hako1 = 0, hako2 = 0;
	int i = 0;
	int hako3 = 0;
	int hako4 = 0;
	int kumi_rev_rank = 20;
	int nokorimono3 = 0;
	int nokorimono4 = 0;

	copyCards(copy_cards, my_cards);

	state.cause_rev = 1;
	bunkatsu_pre2(copy_cards);
	state.cause_rev = 0;

	if (state.kumi_info[13][1] != 0)
	{
		hako1 = state.kumi_info[13][18]; //現時点の手札に得点付け

		kuminumber = max_kumi_info(14); //革命を起こす組を見つける
		kumi_rev_rank = (state.kumi_info[kuminumber][3] + state.kumi_info[kuminumber][4]) / 2;
		//printf("state.kumi_info[3]   %d\n",state.kumi_info[kuminumber][3]);
		//printf("state.kumi_info[4]   %d\n",state.kumi_info[kuminumber][4]);

		//革命を起こす組を一旦手札から取り除く
		if (kuminumber == 0)
			cardsDiff3(copy_cards, state.kumi0);
		else if (kuminumber == 1)
			cardsDiff3(copy_cards, state.kumi1);
		else if (kuminumber == 2)
			cardsDiff3(copy_cards, state.kumi2);
		else if (kuminumber == 3)
			cardsDiff3(copy_cards, state.kumi3);
		else if (kuminumber == 4)
			cardsDiff3(copy_cards, state.kumi4);
		else if (kuminumber == 5)
			cardsDiff3(copy_cards, state.kumi5);
		else if (kuminumber == 6)
			cardsDiff3(copy_cards, state.kumi6);
		else if (kuminumber == 7)
			cardsDiff3(copy_cards, state.kumi7);
		else if (kuminumber == 8)
			cardsDiff3(copy_cards, state.kumi8);
		else if (kuminumber == 9)
			cardsDiff3(copy_cards, state.kumi9);
		else if (kuminumber == 10)
			cardsDiff3(copy_cards, state.kumi10);

		if (kuminumber == 0)
			copyCards(copy_cards3, state.kumi0);
		else if (kuminumber == 1)
			copyCards(copy_cards3, state.kumi1);
		else if (kuminumber == 2)
			copyCards(copy_cards3, state.kumi2);
		else if (kuminumber == 3)
			copyCards(copy_cards3, state.kumi3);
		else if (kuminumber == 4)
			copyCards(copy_cards3, state.kumi4);
		else if (kuminumber == 5)
			copyCards(copy_cards3, state.kumi5);
		else if (kuminumber == 6)
			copyCards(copy_cards3, state.kumi6);
		else if (kuminumber == 7)
			copyCards(copy_cards3, state.kumi7);
		else if (kuminumber == 8)
			copyCards(copy_cards3, state.kumi8);
		else if (kuminumber == 9)
			copyCards(copy_cards3, state.kumi9);
		else if (kuminumber == 10)
			copyCards(copy_cards3, state.kumi10);

		bunkatsu_pre2(copy_cards);
		hako4 = state.kumi_info[13][18];
		nokorimono4 = state.kumi_info[13][0] - state.kumi_info[13][7];

		//その後、場を流せる強いカードの組を見つけ、一旦取り除き、そのうえで、「「革命時の」1枚当たりの強さ評価値の合計」を組数で割った値(hako2)を求める。
		state.rev = 0;
		bunkatsu_pre2(copy_cards);
		state.rev = 1;
		hako3 = state.kumi_info[13][17];
		nokorimono3 = state.kumi_info[13][0] - state.kumi_info[13][6];
		while (1)
		{
			kuminumber = max_kumi_info(4);
			if (state.kumi_info[kuminumber][12] >= STRONG)
			{
				state.kumi_info[kuminumber][1] = 0;
				if (state.kumi_info[kuminumber][3] != 6 && state.kumi_info[kuminumber][4] != 6)
				{

					if (kuminumber == 0)
						cardsDiff3(copy_cards, state.kumi0);
					else if (kuminumber == 1)
						cardsDiff3(copy_cards, state.kumi1);
					else if (kuminumber == 2)
						cardsDiff3(copy_cards, state.kumi2);
					else if (kuminumber == 3)
						cardsDiff3(copy_cards, state.kumi3);
					else if (kuminumber == 4)
						cardsDiff3(copy_cards, state.kumi4);
					else if (kuminumber == 5)
						cardsDiff3(copy_cards, state.kumi5);
					else if (kuminumber == 6)
						cardsDiff3(copy_cards, state.kumi6);
					else if (kuminumber == 7)
						cardsDiff3(copy_cards, state.kumi7);
					else if (kuminumber == 8)
						cardsDiff3(copy_cards, state.kumi8);
					else if (kuminumber == 9)
						cardsDiff3(copy_cards, state.kumi9);
					else if (kuminumber == 10)
						cardsDiff3(copy_cards, state.kumi10);

					if (kuminumber == 0)
						copyCards(copy_cards2, state.kumi0);
					else if (kuminumber == 1)
						copyCards(copy_cards2, state.kumi1);
					else if (kuminumber == 2)
						copyCards(copy_cards2, state.kumi2);
					else if (kuminumber == 3)
						copyCards(copy_cards2, state.kumi3);
					else if (kuminumber == 4)
						copyCards(copy_cards2, state.kumi4);
					else if (kuminumber == 5)
						copyCards(copy_cards2, state.kumi5);
					else if (kuminumber == 6)
						copyCards(copy_cards2, state.kumi6);
					else if (kuminumber == 7)
						copyCards(copy_cards2, state.kumi7);
					else if (kuminumber == 8)
						copyCards(copy_cards2, state.kumi8);
					else if (kuminumber == 9)
						copyCards(copy_cards2, state.kumi9);
					else if (kuminumber == 10)
						copyCards(copy_cards2, state.kumi10);
				}
			}
			else
			{

				break;
			}
		}

		state.rev = 0;
		bunkatsu_pre2(copy_cards);
		state.rev = 1;
		hako2 = state.kumi_info[13][17];

		//何もしなかったときの手札の点数と、手札の流せる札を消費した後の革命後の手札の点数を比較して
		//後のほうがよければ、消費した後に、革命を起こす。
		if ((hako4 <= hako3 + 1) && kumi_rev_rank > 1)
		{ //||nokorimono3<nokorimono4
			copyTable(out_cards, copy_cards2);
			if (beEmptyCards(out_cards) == 1)
				copyTable(out_cards, copy_cards3);
		}

		if (state.kumi_info[13][0] - state.kumi_info[13][6] <= 1)
		{
			copyTable(out_cards, copy_cards2);
			if (beEmptyCards(out_cards) == 1)
				copyTable(out_cards, copy_cards3);
		}
	}
}

void my_lead5_1_rev(int out_cards[8][15], int my_cards[8][15])
{ //革命を用いた勝ち手札の確認

	//革命を起こすことができるか、起こしてよいのか、を判断する

	int kuminumber = 20;
	int copy_cards[8][15] = {{0}};
	int copy_cards2[8][15] = {{0}};
	int copy_cards3[8][15] = {{0}};
	int temp[8][15] = {{0}};
	int hako1 = 0, hako2 = 0;
	int i = 0;
	int hako3 = 0;
	int hako4 = 0;
	int kumi_rev_rank = 20;
	int nokorimono3 = 0;
	int nokorimono4 = 0;

	copyCards(copy_cards, my_cards);

	state.cause_rev = 1;
	bunkatsu_pre2(copy_cards);
	state.cause_rev = 0;

	if (state.kumi_info[13][1] != 0)
	{
		hako1 = state.kumi_info[13][18]; //現時点の手札に得点付け

		kuminumber = max_kumi_info(14); //革命を起こす組を見つける
		kumi_rev_rank = (state.kumi_info[kuminumber][3] + state.kumi_info[kuminumber][4]) / 2;
		//printf("state.kumi_info[3]   %d\n",state.kumi_info[kuminumber][3]);
		//printf("state.kumi_info[4]   %d\n",state.kumi_info[kuminumber][4]);

		//革命を起こす組を一旦手札から取り除く
		if (kuminumber == 0)
			cardsDiff3(copy_cards, state.kumi0);
		else if (kuminumber == 1)
			cardsDiff3(copy_cards, state.kumi1);
		else if (kuminumber == 2)
			cardsDiff3(copy_cards, state.kumi2);
		else if (kuminumber == 3)
			cardsDiff3(copy_cards, state.kumi3);
		else if (kuminumber == 4)
			cardsDiff3(copy_cards, state.kumi4);
		else if (kuminumber == 5)
			cardsDiff3(copy_cards, state.kumi5);
		else if (kuminumber == 6)
			cardsDiff3(copy_cards, state.kumi6);
		else if (kuminumber == 7)
			cardsDiff3(copy_cards, state.kumi7);
		else if (kuminumber == 8)
			cardsDiff3(copy_cards, state.kumi8);
		else if (kuminumber == 9)
			cardsDiff3(copy_cards, state.kumi9);
		else if (kuminumber == 10)
			cardsDiff3(copy_cards, state.kumi10);

		if (kuminumber == 0)
			copyCards(copy_cards3, state.kumi0);
		else if (kuminumber == 1)
			copyCards(copy_cards3, state.kumi1);
		else if (kuminumber == 2)
			copyCards(copy_cards3, state.kumi2);
		else if (kuminumber == 3)
			copyCards(copy_cards3, state.kumi3);
		else if (kuminumber == 4)
			copyCards(copy_cards3, state.kumi4);
		else if (kuminumber == 5)
			copyCards(copy_cards3, state.kumi5);
		else if (kuminumber == 6)
			copyCards(copy_cards3, state.kumi6);
		else if (kuminumber == 7)
			copyCards(copy_cards3, state.kumi7);
		else if (kuminumber == 8)
			copyCards(copy_cards3, state.kumi8);
		else if (kuminumber == 9)
			copyCards(copy_cards3, state.kumi9);
		else if (kuminumber == 10)
			copyCards(copy_cards3, state.kumi10);

		bunkatsu_pre2(copy_cards);
		hako4 = state.kumi_info[13][18];
		nokorimono4 = state.kumi_info[13][0] - state.kumi_info[13][7];

		//その後、場を流せる強いカードの組を見つけ、一旦取り除き、そのうえで、「「革命時の」1枚当たりの強さ評価値の合計」を組数で割った値(hako2)を求める。
		state.rev = 0;
		bunkatsu_pre2(copy_cards);
		state.rev = 1;
		hako3 = state.kumi_info[13][17];
		nokorimono3 = state.kumi_info[13][0] - state.kumi_info[13][6];
		while (1)
		{
			kuminumber = max_kumi_info(4);
			if (state.kumi_info[kuminumber][12] >= STRONG)
			{
				state.kumi_info[kuminumber][1] = 0;
				if (state.kumi_info[kuminumber][3] != 6 && state.kumi_info[kuminumber][4] != 6)
				{

					if (kuminumber == 0)
						cardsDiff3(copy_cards, state.kumi0);
					else if (kuminumber == 1)
						cardsDiff3(copy_cards, state.kumi1);
					else if (kuminumber == 2)
						cardsDiff3(copy_cards, state.kumi2);
					else if (kuminumber == 3)
						cardsDiff3(copy_cards, state.kumi3);
					else if (kuminumber == 4)
						cardsDiff3(copy_cards, state.kumi4);
					else if (kuminumber == 5)
						cardsDiff3(copy_cards, state.kumi5);
					else if (kuminumber == 6)
						cardsDiff3(copy_cards, state.kumi6);
					else if (kuminumber == 7)
						cardsDiff3(copy_cards, state.kumi7);
					else if (kuminumber == 8)
						cardsDiff3(copy_cards, state.kumi8);
					else if (kuminumber == 9)
						cardsDiff3(copy_cards, state.kumi9);
					else if (kuminumber == 10)
						cardsDiff3(copy_cards, state.kumi10);

					if (kuminumber == 0)
						copyCards(copy_cards2, state.kumi0);
					else if (kuminumber == 1)
						copyCards(copy_cards2, state.kumi1);
					else if (kuminumber == 2)
						copyCards(copy_cards2, state.kumi2);
					else if (kuminumber == 3)
						copyCards(copy_cards2, state.kumi3);
					else if (kuminumber == 4)
						copyCards(copy_cards2, state.kumi4);
					else if (kuminumber == 5)
						copyCards(copy_cards2, state.kumi5);
					else if (kuminumber == 6)
						copyCards(copy_cards2, state.kumi6);
					else if (kuminumber == 7)
						copyCards(copy_cards2, state.kumi7);
					else if (kuminumber == 8)
						copyCards(copy_cards2, state.kumi8);
					else if (kuminumber == 9)
						copyCards(copy_cards2, state.kumi9);
					else if (kuminumber == 10)
						copyCards(copy_cards2, state.kumi10);
				}
			}
			else
			{

				break;
			}
		}

		state.rev = 0;
		bunkatsu_pre2(copy_cards);
		state.rev = 1;
		hako2 = state.kumi_info[13][17];

		//何もしなかったときの手札の点数と、手札の流せる札を消費した後の革命後の手札の点数を比較して
		//後のほうがよければ、消費した後に、革命を起こす。
		/*
		if((hako4<=hako3+1)&&kumi_rev_rank>1){//||nokorimono3<nokorimono4
			copyTable(out_cards,copy_cards2);
			if(beEmptyCards(out_cards)==1)copyTable(out_cards,copy_cards3);	
		}
		*/

		if (state.kumi_info[13][0] - state.kumi_info[13][6] <= 1)
		{
			copyTable(out_cards, copy_cards2);
			if (beEmptyCards(out_cards) == 1)
				copyTable(out_cards, copy_cards3);
		}
	}
}

int my_lead5_rev_4(int out_cards[8][15], int my_cards[8][15])
{

	//革命を起こすことができるか、起こしてよいのか、を判断する

	int kuminumber = 20;
	int copy_cards[8][15] = {{0}};
	int copy_cards2[8][15] = {{0}};
	int copy_cards3[8][15] = {{0}};
	int temp[8][15] = {{0}};
	int hako1 = 0, hako2 = 0;
	int i = 0;
	int hako3 = 0;
	int hako4 = 0;
	int kumi_rev_rank = 20;
	int nokorimono3 = 0;
	int nokorimono4 = 0;

	copyCards(copy_cards, my_cards);

	state.cause_rev = 1;
	bunkatsu_pre2(copy_cards);
	state.cause_rev = 0;

	if (state.kumi_info[13][1] != 0)
	{
		hako1 = state.kumi_info[13][18]; //現時点の手札に得点付け

		kuminumber = max_kumi_info(14); //革命を起こす組を見つける
		kumi_rev_rank = (state.kumi_info[kuminumber][3] + state.kumi_info[kuminumber][4]) / 2;
		//printf("state.kumi_info[3]   %d\n",state.kumi_info[kuminumber][3]);
		//printf("state.kumi_info[4]   %d\n",state.kumi_info[kuminumber][4]);

		//革命を起こす組を一旦手札から取り除く
		if (kuminumber == 0)
			cardsDiff3(copy_cards, state.kumi0);
		else if (kuminumber == 1)
			cardsDiff3(copy_cards, state.kumi1);
		else if (kuminumber == 2)
			cardsDiff3(copy_cards, state.kumi2);
		else if (kuminumber == 3)
			cardsDiff3(copy_cards, state.kumi3);
		else if (kuminumber == 4)
			cardsDiff3(copy_cards, state.kumi4);
		else if (kuminumber == 5)
			cardsDiff3(copy_cards, state.kumi5);
		else if (kuminumber == 6)
			cardsDiff3(copy_cards, state.kumi6);
		else if (kuminumber == 7)
			cardsDiff3(copy_cards, state.kumi7);
		else if (kuminumber == 8)
			cardsDiff3(copy_cards, state.kumi8);
		else if (kuminumber == 9)
			cardsDiff3(copy_cards, state.kumi9);
		else if (kuminumber == 10)
			cardsDiff3(copy_cards, state.kumi10);

		if (kuminumber == 0)
			copyCards(copy_cards3, state.kumi0);
		else if (kuminumber == 1)
			copyCards(copy_cards3, state.kumi1);
		else if (kuminumber == 2)
			copyCards(copy_cards3, state.kumi2);
		else if (kuminumber == 3)
			copyCards(copy_cards3, state.kumi3);
		else if (kuminumber == 4)
			copyCards(copy_cards3, state.kumi4);
		else if (kuminumber == 5)
			copyCards(copy_cards3, state.kumi5);
		else if (kuminumber == 6)
			copyCards(copy_cards3, state.kumi6);
		else if (kuminumber == 7)
			copyCards(copy_cards3, state.kumi7);
		else if (kuminumber == 8)
			copyCards(copy_cards3, state.kumi8);
		else if (kuminumber == 9)
			copyCards(copy_cards3, state.kumi9);
		else if (kuminumber == 10)
			copyCards(copy_cards3, state.kumi10);

		bunkatsu_pre2(copy_cards);
		hako4 = state.kumi_info[13][18];
		nokorimono4 = state.kumi_info[13][0] - state.kumi_info[13][7];

		//その後、場を流せる強いカードの組を見つけ、一旦取り除き、そのうえで、「「革命時の」1枚当たりの強さ評価値の合計」を組数で割った値(hako2)を求める。
		state.rev = 0;
		bunkatsu_pre2(copy_cards);
		state.rev = 1;
		hako3 = state.kumi_info[13][17];
		nokorimono3 = state.kumi_info[13][0] - state.kumi_info[13][6];
		while (1)
		{
			kuminumber = max_kumi_info(4);
			if (state.kumi_info[kuminumber][12] >= STRONG)
			{
				state.kumi_info[kuminumber][1] = 0;
				if (state.kumi_info[kuminumber][3] != 6 && state.kumi_info[kuminumber][4] != 6)
				{

					if (kuminumber == 0)
						cardsDiff3(copy_cards, state.kumi0);
					else if (kuminumber == 1)
						cardsDiff3(copy_cards, state.kumi1);
					else if (kuminumber == 2)
						cardsDiff3(copy_cards, state.kumi2);
					else if (kuminumber == 3)
						cardsDiff3(copy_cards, state.kumi3);
					else if (kuminumber == 4)
						cardsDiff3(copy_cards, state.kumi4);
					else if (kuminumber == 5)
						cardsDiff3(copy_cards, state.kumi5);
					else if (kuminumber == 6)
						cardsDiff3(copy_cards, state.kumi6);
					else if (kuminumber == 7)
						cardsDiff3(copy_cards, state.kumi7);
					else if (kuminumber == 8)
						cardsDiff3(copy_cards, state.kumi8);
					else if (kuminumber == 9)
						cardsDiff3(copy_cards, state.kumi9);
					else if (kuminumber == 10)
						cardsDiff3(copy_cards, state.kumi10);

					if (kuminumber == 0)
						copyCards(copy_cards2, state.kumi0);
					else if (kuminumber == 1)
						copyCards(copy_cards2, state.kumi1);
					else if (kuminumber == 2)
						copyCards(copy_cards2, state.kumi2);
					else if (kuminumber == 3)
						copyCards(copy_cards2, state.kumi3);
					else if (kuminumber == 4)
						copyCards(copy_cards2, state.kumi4);
					else if (kuminumber == 5)
						copyCards(copy_cards2, state.kumi5);
					else if (kuminumber == 6)
						copyCards(copy_cards2, state.kumi6);
					else if (kuminumber == 7)
						copyCards(copy_cards2, state.kumi7);
					else if (kuminumber == 8)
						copyCards(copy_cards2, state.kumi8);
					else if (kuminumber == 9)
						copyCards(copy_cards2, state.kumi9);
					else if (kuminumber == 10)
						copyCards(copy_cards2, state.kumi10);
				}
			}
			else
			{

				break;
			}
		}

		state.rev = 0;
		bunkatsu_pre2(copy_cards);
		state.rev = 1;
		hako2 = state.kumi_info[13][17];

		//何もしなかったときの手札の点数と、手札の流せる札を消費した後の革命後の手札の点数を比較して
		//後のほうがよければ、消費した後に、革命を起こす。
		/*
		if((hako4<=hako3+1)&&kumi_rev_rank>1){//||nokorimono3<nokorimono4
			copyTable(out_cards,copy_cards2);
			if(beEmptyCards(out_cards)==1)copyTable(out_cards,copy_cards3);	
		}
		*/

		if (state.kumi_info[13][0] - state.kumi_info[13][6] <= 1)
		{
			copyTable(out_cards, copy_cards2);
			if (beEmptyCards(out_cards) == 1)
				copyTable(out_cards, copy_cards3);
			return 1;
		}
		return 0;
	}
	return 0;
}

void my_lead6_rev(int out_cards[8][15], int my_cards[8][15])
{

	int kuminumber = 0;
	int copy_cards[8][15] = {{0}};
	int hokan = 20;
	int mark = 20;

	int hokan3 = 20;
	int flag = 0;

	copyCards(copy_cards, my_cards);
	bunkatsu_pre2(copy_cards);

	kuminumber = max_kumi_info_kakou(4);

	if (state.kumi_info[kuminumber][3] == 6 && state.kumi_info[13][0] >= 4 && state.kumi_info[kuminumber][0] != 3 && qtyOfCards8(state.rest_cards, 6) >= 2)
	{ //場に何もないときに8を出そうとしたら
		state.kumi_info[kuminumber][1] = 0;
		kuminumber = max_kumi_info_kakou(4);
	}
	if (state.kumi_info[kuminumber][3] == 6 && state.kumi_info[13][0] == 3 && state.kumi_info[kuminumber][0] == 2 && qtyOfCards8(state.rest_cards, 6) >= 2)
	{ //場に何もないときに8を出そうとしたら
		state.kumi_info[kuminumber][1] = 0;
		kuminumber = max_kumi_info_kakou(4);
	}
	if (state.kumi_info[kuminumber][3] == 6 && state.kumi_info[13][0] == 3 && state.kumi_info[kuminumber][0] == 1 && qtyOfCards8(state.rest_cards, 6) >= 2 && state.kumi_info[13][9] < 2)
	{ //場に何もないときに8を出そうとしたら
		state.kumi_info[kuminumber][1] = 0;
		kuminumber = max_kumi_info_kakou(4);
	}

	if (state.kumi_info[kuminumber][4] > 6 && state.kumi_info[13][0] == 3 && qtyOfCards5(copy_cards, 6) >= 1)
	{ //&&state.kumi_info[kuminumber][0]==1
		hokan = kuminumber;
		//mark=state.kumi_info[kuminumber][2];

		state.kumi_info[kuminumber][1] = 0;
		kuminumber = max_kumi_info_kakou(4);
		if (state.kumi_info[kuminumber][3] == 6 && state.kumi_info[kuminumber][0] == 1)
		{
			hokan3 = kuminumber;
			state.kumi_info[kuminumber][1] = 0;
			kuminumber = max_kumi_info_kakou(4);
			if (state.kumi_info[kuminumber][3] <= 4 && state.kumi_info[kuminumber][0] == 1)
			{
				kuminumber = hokan3; //確認の末、8単体を提出。
				printf("my_lead6_rev test3\n");
				flag = 1;
			}
		}
		if (flag == 0)
		{
			kuminumber = hokan;
		}
	}
	/*
	if(state.kumi_info[kuminumber][4]>6&&state.kumi_info[13][0]==3&&qtyOfCards5(copy_cards,6)>=1&&qtyOfCards8(state.rest_cards,state.kumi_info[kuminumber][4])<=0){
			hokan=kuminumber;
			mark=state.kumi_info[kuminumber][2];
		
			state.kumi_info[kuminumber][1]=0;
			kuminumber=max_kumi_info_kakou(4);
			if(state.kumi_info[kuminumber][3]==6&&state.kumi_info[kuminumber][0]==1){
				printf("my_lead6_rev test4\n");
			}else{
				kuminumber=hokan;
			}
	}
	*/
	/*
	if(state.kumi_info[kuminumber][4]>10&&state.kumi_info[13][0]==3&&qtyOfCards5(copy_cards,6)>=1){
			state.kumi_info[kuminumber][1]=0;
			hokan=kuminumber;
			kuminumber=max_kumi_info_kakou(4);
			if(state.kumi_info[kuminumber][3]==6&&state.kumi_info[kuminumber][0]==1){
				printf("my_lead6_rev test2\n");
			}else{
				kuminumber=hokan;
			}
	}
	*/

	if (kuminumber == 0)
		copyTable(out_cards, state.kumi0);
	else if (kuminumber == 1)
		copyTable(out_cards, state.kumi1);
	else if (kuminumber == 2)
		copyTable(out_cards, state.kumi2);
	else if (kuminumber == 3)
		copyTable(out_cards, state.kumi3);
	else if (kuminumber == 4)
		copyTable(out_cards, state.kumi4);
	else if (kuminumber == 5)
		copyTable(out_cards, state.kumi5);
	else if (kuminumber == 6)
		copyTable(out_cards, state.kumi6);
	else if (kuminumber == 7)
		copyTable(out_cards, state.kumi7);
	else if (kuminumber == 8)
		copyTable(out_cards, state.kumi8);
	else if (kuminumber == 9)
		copyTable(out_cards, state.kumi9);
	else if (kuminumber == 10)
		copyTable(out_cards, state.kumi10);

	//printf("ato %d\n",kuminumber);
}

void my_lead7_rev(int out_cards[8][15], int my_cards[8][15])
{

	int kuminumber = 0;
	int copy_cards[8][15] = {{0}};

	copyCards(copy_cards, my_cards);
	bunkatsu_pre2(copy_cards);

	kuminumber = max_kumi_info_kakou(4);
	if (kuminumber == 0)
		copyTable(out_cards, state.kumi0);
	else if (kuminumber == 1)
		copyTable(out_cards, state.kumi1);
	else if (kuminumber == 2)
		copyTable(out_cards, state.kumi2);
	else if (kuminumber == 3)
		copyTable(out_cards, state.kumi3);
	else if (kuminumber == 4)
		copyTable(out_cards, state.kumi4);
	else if (kuminumber == 5)
		copyTable(out_cards, state.kumi5);
	else if (kuminumber == 6)
		copyTable(out_cards, state.kumi6);
	else if (kuminumber == 7)
		copyTable(out_cards, state.kumi7);
	else if (kuminumber == 8)
		copyTable(out_cards, state.kumi8);
	else if (kuminumber == 9)
		copyTable(out_cards, state.kumi9);
	else if (kuminumber == 10)
		copyTable(out_cards, state.kumi10);
}

void my_lead8_rev(int out_cards[8][15], int my_cards[8][15])
{

	int kuminumber = 0;
	int copy_cards[8][15] = {{0}};
	int maisuu = 0;

	copyCards(copy_cards, my_cards);
	bunkatsu_pre2(copy_cards);

	if (state.kumi_info[13][0] - state.kumi_info[13][7] == 2)
	{ //

		kuminumber = min_kumi_info(12);
		state.kumi_info[kuminumber][1] = 0;
		//最弱の評価値の組を見つけて、消して、

		kuminumber = min_kumi_info(12);
		maisuu = state.kumi_info[kuminumber][1];
		//2番目に弱い評価値の組を見つける。

		if (maisuu == 1 && state.kumi_info[13][24] + state.kumi_info[13][34] >= 1)
		{ //2番目に弱い組の構成枚数と同じ構成枚数かつ流し評価値が規定値を超えている組があるかを確認

			if (kuminumber == 0)
				copyTable(out_cards, state.kumi0);
			else if (kuminumber == 1)
				copyTable(out_cards, state.kumi1);
			else if (kuminumber == 2)
				copyTable(out_cards, state.kumi2);
			else if (kuminumber == 3)
				copyTable(out_cards, state.kumi3);
			else if (kuminumber == 4)
				copyTable(out_cards, state.kumi4);
			else if (kuminumber == 5)
				copyTable(out_cards, state.kumi5);
			else if (kuminumber == 6)
				copyTable(out_cards, state.kumi6);
			else if (kuminumber == 7)
				copyTable(out_cards, state.kumi7);
			else if (kuminumber == 8)
				copyTable(out_cards, state.kumi8);
			else if (kuminumber == 9)
				copyTable(out_cards, state.kumi9);
			else if (kuminumber == 10)
				copyTable(out_cards, state.kumi10);
		}

		if (maisuu == 2 && state.kumi_info[13][26] + state.kumi_info[13][34] - state.kumi_info[13][47] >= 1)
		{ //2番目に弱い組の構成枚数と同じ構成枚数かつ流し評価値が規定値を超えている組があるかを確認

			if (kuminumber == 0)
				copyTable(out_cards, state.kumi0);
			else if (kuminumber == 1)
				copyTable(out_cards, state.kumi1);
			else if (kuminumber == 2)
				copyTable(out_cards, state.kumi2);
			else if (kuminumber == 3)
				copyTable(out_cards, state.kumi3);
			else if (kuminumber == 4)
				copyTable(out_cards, state.kumi4);
			else if (kuminumber == 5)
				copyTable(out_cards, state.kumi5);
			else if (kuminumber == 6)
				copyTable(out_cards, state.kumi6);
			else if (kuminumber == 7)
				copyTable(out_cards, state.kumi7);
			else if (kuminumber == 8)
				copyTable(out_cards, state.kumi8);
			else if (kuminumber == 9)
				copyTable(out_cards, state.kumi9);
			else if (kuminumber == 10)
				copyTable(out_cards, state.kumi10);
		}
	}
}

void my_lead9_rev(int out_cards[8][15], int my_cards[8][15])
{

	int kuminumber = 0;
	int copy_cards[8][15] = {{0}};
	int hokan = 20;
	int mark = 20;

	int hokan3 = 20;
	int flag = 0;

	copyCards(copy_cards, my_cards);
	bunkatsu_pre2(copy_cards);

	if (state.kumi_info[13][24] + state.kumi_info[13][26] + state.kumi_info[13][34] == 0)
	{

		kuminumber = max_kumi_info_kakou2(4);

		if (state.kumi_info[kuminumber][3] == 6 && state.kumi_info[13][0] >= 4 && state.kumi_info[kuminumber][0] != 3 && qtyOfCards8(state.rest_cards, 6) >= 2)
		{
			state.kumi_info[kuminumber][1] = 0;
			kuminumber = max_kumi_info_kakou2(4);
		}

		if (state.kumi_info[kuminumber][3] == 6 && state.kumi_info[13][0] == 3 && state.kumi_info[kuminumber][0] == 2 && qtyOfCards8(state.rest_cards, 6) >= 2)
		{
			state.kumi_info[kuminumber][1] = 0;
			kuminumber = max_kumi_info_kakou2(4);
		}

		if (state.kumi_info[kuminumber][3] == 6 && state.kumi_info[13][0] == 3 && state.kumi_info[kuminumber][0] == 1 && qtyOfCards8(state.rest_cards, 6) >= 2 && state.kumi_info[13][9] < 2)
		{ //場に何もないときに8を出そうとしたら
			state.kumi_info[kuminumber][1] = 0;
			kuminumber = max_kumi_info_kakou2(4);
		}

		if (state.kumi_info[kuminumber][4] > 6 && state.kumi_info[13][0] == 3 && qtyOfCards5(copy_cards, 6) >= 1)
		{ //&&state.kumi_info[kuminumber][0]==1
			hokan = kuminumber;
			mark = state.kumi_info[kuminumber][2];

			state.kumi_info[kuminumber][1] = 0;
			kuminumber = max_kumi_info_kakou2(4);
			if (state.kumi_info[kuminumber][3] == 6 && state.kumi_info[kuminumber][0] == 1)
			{
				printf("my_lead9_rev test3\n");
			}
			else
			{
				kuminumber = hokan;
			}
		}

		if (state.kumi_info[kuminumber][4] < 6 && state.kumi_info[13][0] == 3 && qtyOfCards5(copy_cards, 6) >= 1)
		{ //&&state.kumi_info[kuminumber][0]==1
			hokan = kuminumber;
			//mark=state.kumi_info[kuminumber][2];

			state.kumi_info[kuminumber][1] = 0;
			kuminumber = max_kumi_info_kakou2(4);
			if (state.kumi_info[kuminumber][3] == 6 && state.kumi_info[kuminumber][0] == 1)
			{
				hokan3 = kuminumber;
				state.kumi_info[kuminumber][1] = 0;
				kuminumber = max_kumi_info_kakou2(4);
				if (state.kumi_info[kuminumber][3] <= 4 && state.kumi_info[kuminumber][0] == 1)
				{
					kuminumber = hokan3; //確認の末、8単体を提出。
					printf("my_lead9 test3\n");
					flag = 1;
				}
			}
			if (flag == 0)
			{
				kuminumber = hokan;
			}
		}
		/*
		if(state.kumi_info[kuminumber][4]>6&&state.kumi_info[13][0]==3&&qtyOfCards5(copy_cards,6)>=1&&qtyOfCards8(state.rest_cards,state.kumi_info[kuminumber][4])<=0){
			hokan=kuminumber;
			mark=state.kumi_info[kuminumber][2];
		
			state.kumi_info[kuminumber][1]=0;
			kuminumber=max_kumi_info_kakou2(4);
			if(state.kumi_info[kuminumber][3]==6&&state.kumi_info[kuminumber][0]==1){
				printf("my_lead9_rev test4\n");
			}else{
				kuminumber=hokan;
			}
		}
		*/
		/*
		if(state.kumi_info[kuminumber][4]>10&&state.kumi_info[13][0]==3&&qtyOfCards5(copy_cards,6)>=1){
			state.kumi_info[kuminumber][1]=0;
			hokan=kuminumber;
			kuminumber=max_kumi_info_kakou2(4);
			if(state.kumi_info[kuminumber][3]==6&&state.kumi_info[kuminumber][0]==1){
				printf("my_lead9_rev test2\n");
			}else{
				kuminumber=hokan;
			}
		}
		*/

		if (kuminumber == 0)
			copyTable(out_cards, state.kumi0);
		else if (kuminumber == 1)
			copyTable(out_cards, state.kumi1);
		else if (kuminumber == 2)
			copyTable(out_cards, state.kumi2);
		else if (kuminumber == 3)
			copyTable(out_cards, state.kumi3);
		else if (kuminumber == 4)
			copyTable(out_cards, state.kumi4);
		else if (kuminumber == 5)
			copyTable(out_cards, state.kumi5);
		else if (kuminumber == 6)
			copyTable(out_cards, state.kumi6);
		else if (kuminumber == 7)
			copyTable(out_cards, state.kumi7);
		else if (kuminumber == 8)
			copyTable(out_cards, state.kumi8);
		else if (kuminumber == 9)
			copyTable(out_cards, state.kumi9);
		else if (kuminumber == 10)
			copyTable(out_cards, state.kumi10);
	}
}

void my_lead10_rev(int out_cards[8][15], int my_cards[8][15])
{

	int kuminumber = 14;
	int copy_cards[8][15] = {{0}};
	int hand_kumi = 0, nagashi_kumi = 0, check = 0;

	int flag_eight = 0;
	int hokan = 20;
	int mark = 20;

	copyCards(copy_cards, my_cards);
	bunkatsu_pre2(copy_cards);
	hand_kumi = state.kumi_info[13][0];
	nagashi_kumi = state.kumi_info[13][35];																			//99
	check = hand_kumi - nagashi_kumi - state.kumi_info[13][36] - state.kumi_info[13][38] - state.kumi_info[13][34]; //規定値－1以上の2を含まない1枚組と2枚組の数と2を含む階段以外の組の数

	if (check <= 1)
	{

		flag_eight = qtyOfCards5(my_cards, 6);

		kuminumber = max_kumi_info(4);

		//if(state.kumi_info[kuminumber][1]>=3){
		/*
			if(kuminumber==0)copyTable(out_cards,state.kumi0);
			else if(kuminumber==1)copyTable(out_cards,state.kumi1);
			else if(kuminumber==2)copyTable(out_cards,state.kumi2);
			else if(kuminumber==3)copyTable(out_cards,state.kumi3);
			else if(kuminumber==4)copyTable(out_cards,state.kumi4);
			else if(kuminumber==5)copyTable(out_cards,state.kumi5);
			else if(kuminumber==6)copyTable(out_cards,state.kumi6);
			else if(kuminumber==7)copyTable(out_cards,state.kumi7);
			else if(kuminumber==8)copyTable(out_cards,state.kumi8);
			else if(kuminumber==9)copyTable(out_cards,state.kumi9);
			else if(kuminumber==10)copyTable(out_cards,state.kumi10);
			*/

		//}else{

		kuminumber = min_kumi_info(12);

		state.kumi_info[kuminumber][1] = 0;

		kuminumber = max_kumi_info_kakou(4);

		if (state.kumi_info[kuminumber][3] == 6 && state.kumi_info[13][0] >= 3 && state.kumi_info[kuminumber][0] != 3)
		{ //場に何もないときに8を出そうとしたら
			state.kumi_info[kuminumber][1] = 0;
			kuminumber = max_kumi_info_kakou(4);
		}
		/*
			if(state.kumi_info[kuminumber][3]==6&&state.kumi_info[13][0]==3&&state.kumi_info[kuminumber][0]==2&&qtyOfCards8(state.rest_cards,6)>=2){//場に何もないときに8を出そうとしたら
					state.kumi_info[kuminumber][1]=0;
					kuminumber=max_kumi_info_kakou(4);		
			}
			
			if(state.kumi_info[kuminumber][3]==6&&state.kumi_info[13][0]==3&&state.kumi_info[kuminumber][0]==1&&qtyOfCards8(state.rest_cards,6)>=2&&state.kumi_info[13][9]<2){//場に何もないときに8を出そうとしたら
				state.kumi_info[kuminumber][1]=0;
				kuminumber=max_kumi_info_kakou(4);
			}
			
			if(state.kumi_info[kuminumber][4]>6&&state.kumi_info[13][0]==3&&qtyOfCards5(copy_cards,6)>=1&&state.kumi_info[kuminumber][0]==1){
				hokan=kuminumber;
				mark=state.kumi_info[kuminumber][2];
			
				state.kumi_info[kuminumber][1]=0;
				kuminumber=max_kumi_info_kakou(4);
				if(state.kumi_info[kuminumber][3]==6&&state.kumi_info[kuminumber][0]==1){
					printf("my_lead6_rev test3\n");
				}else{
					kuminumber=hokan;
				}
			}
			
			if(state.kumi_info[kuminumber][4]>6&&state.kumi_info[13][0]==3&&qtyOfCards5(copy_cards,6)>=1&&qtyOfCards8(state.rest_cards,state.kumi_info[kuminumber][4])<=0){
				hokan=kuminumber;
				mark=state.kumi_info[kuminumber][2];
			
				state.kumi_info[kuminumber][1]=0;
				kuminumber=max_kumi_info_kakou(4);
				if(state.kumi_info[kuminumber][3]==6&&state.kumi_info[kuminumber][0]==1){
					printf("my_lead6_rev test3\n");
				}else{
					kuminumber=hokan;
				}
			}
			*/
		if (kuminumber == 0)
			copyTable(out_cards, state.kumi0);
		else if (kuminumber == 1)
			copyTable(out_cards, state.kumi1);
		else if (kuminumber == 2)
			copyTable(out_cards, state.kumi2);
		else if (kuminumber == 3)
			copyTable(out_cards, state.kumi3);
		else if (kuminumber == 4)
			copyTable(out_cards, state.kumi4);
		else if (kuminumber == 5)
			copyTable(out_cards, state.kumi5);
		else if (kuminumber == 6)
			copyTable(out_cards, state.kumi6);
		else if (kuminumber == 7)
			copyTable(out_cards, state.kumi7);
		else if (kuminumber == 8)
			copyTable(out_cards, state.kumi8);
		else if (kuminumber == 9)
			copyTable(out_cards, state.kumi9);
		else if (kuminumber == 10)
			copyTable(out_cards, state.kumi10);

		if (state.kumi_info[13][0] == 2 && state.kumi_info[kuminumber][1] == 1)
			clearCards(out_cards);
		if (state.kumi_info[13][0] == 2 && state.kumi_info[kuminumber][1] == 2 && state.player_number <= 3)
			clearCards(out_cards); //&&state.player_number<=3
		if (state.kumi_info[13][0] == 2 && state.kumi_info[kuminumber][1] >= 3 && state.player_number <= 2)
			clearCards(out_cards); //&&state.player_number<=2

		if (flag_eight >= 1 && state.kumi_info[13][0] == 3 && state.kumi_info[kuminumber][1] == 1)
			clearCards(out_cards);
		if (flag_eight >= 1 && state.kumi_info[13][0] == 3 && state.kumi_info[kuminumber][1] == 2 && state.player_number <= 2)
			clearCards(out_cards); //&&state.player_number<=3
		if (flag_eight >= 1 && state.kumi_info[13][0] == 3 && state.kumi_info[kuminumber][1] >= 3 && state.player_number <= 2)
			clearCards(out_cards); //&&state.player_number<=2
								   //}
	}
}

void my_lead11_rev(int out_cards[8][15], int my_cards[8][15])
{

	int kuminumber = 0;
	int copy_cards[8][15] = {{0}};
	int copy_cards2[8][15] = {{0}};
	int copy_cards3[8][15] = {{0}};

	int o_cards[8][15] = {{0}};
	int o_cards2[8][15] = {{0}};

	int cards[8][15] = {{0}};
	int cards2[8][15] = {{0}};

	int tsuyosa = 0;
	int tsuyosa2 = 0;
	int tsuyosa3 = 0;

	int flag = 0;
	int max_tsuyosa = 0;
	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};
	int h = 7;

	int kumi_suit = 0;
	int kumi_suitsum = 0;
	int bunkatsu_flag = 0;

	int count = 0;

	copyCards(copy_cards, my_cards);
	bunkatsu_pre2(copy_cards);

	while (count < 11)
	{
		if (count == 0)
			copyTable(temp, state.kumi0);
		else if (count == 1)
			copyTable(temp, state.kumi1);
		else if (count == 2)
			copyTable(temp, state.kumi2);
		else if (count == 3)
			copyTable(temp, state.kumi3);
		else if (count == 4)
			copyTable(temp, state.kumi4);
		else if (count == 5)
			copyTable(temp, state.kumi5);
		else if (count == 6)
			copyTable(temp, state.kumi6);
		else if (count == 7)
			copyTable(temp, state.kumi7);
		else if (count == 8)
			copyTable(temp, state.kumi8);
		else if (count == 9)
			copyTable(temp, state.kumi9);
		else if (count == 10)
			copyTable(temp, state.kumi10);

		//
		if (beEmptyCards(temp) == 0)
		{

			copyTable(temp2, temp);
			copyCards(copy_cards, my_cards);
			if (bunkatsu_flag == 1)
			{
				bunkatsu_pre2(copy_cards);
				bunkatsu_flag = 0;
			}

			flag = 0; //リセット
			clearTable(o_cards);
			clearTable(o_cards2);
			clearTable(cards);
			clearTable(cards2);
			tsuyosa2 = 0;
			tsuyosa3 = 0;

			getField2(temp2);

			if (state.kumi_info[count][0] == 1)
			{
				tsuyosa = setvalue_single2(temp2);

				cardsDiff3(copy_cards, temp2);

				copyCards(copy_cards2, copy_cards);
				copyCards(copy_cards3, copy_cards);

				follow_o_shibari_rev(o_cards, state.rest_cards);
				follow_o2_rev(o_cards2, state.rest_cards);

				if (qtyOfCards(o_cards) == qtyOfCards(state.rest_cards))
					flag = 99;

				if (qtyOfCards(o_cards2) == qtyOfCards(state.rest_cards))
					flag = 99;

				if (qtyOfCards(o_cards) == 0)
				{
					flag++;
					tsuyosa2 = STRONG;
				}
				else
				{
					getField2(o_cards);
					follow_o_shibari_rev(cards, copy_cards);
					if (qtyOfCards(cards) != 0)
					{
						flag++;
						kumi_suit = get_suit(cards);
						tsuyosa2 = setvalue_single_shibari(cards, kumi_suit);
					}
				}

				if (qtyOfCards(o_cards2) == 0)
				{
					flag++;
					tsuyosa3 = STRONG;
				}
				else
				{
					getField2(o_cards2);
					follow_o2_rev(cards2, copy_cards);
					if (qtyOfCards(cards2) != 0)
					{
						flag++;
						tsuyosa3 = setvalue_single2(cards2);
					}
				}
			}

			if (state.kumi_info[count][0] == 2)
			{
				tsuyosa = setvalue_pair2(temp2);

				cardsDiff3(copy_cards, temp2);

				copyCards(copy_cards2, copy_cards);
				copyCards(copy_cards3, copy_cards);

				follow_o_shibari_rev(o_cards, state.rest_cards);
				follow_o2_rev(o_cards2, state.rest_cards);

				if (state.player_number == 2)
				{
					if (qtyOfCards(o_cards) == qtyOfCards(state.rest_cards))
						flag = 99;
					if (qtyOfCards(o_cards2) == qtyOfCards(state.rest_cards))
						flag = 99;
				}

				if (qtyOfCards(o_cards) == 0)
				{
					flag++;
					tsuyosa2 = STRONG;
				}
				else
				{
					getField2(o_cards);
					follow_o_shibari_rev(cards, copy_cards);
					if (qtyOfCards(cards) != 0)
					{
						flag++;
						kumi_suitsum = get_suitsum(cards);
						tsuyosa2 = setvalue_pair_shibari(cards, kumi_suitsum);
					}
				}

				if (qtyOfCards(o_cards2) == 0)
				{
					flag++;
					tsuyosa3 = STRONG;
				}
				else
				{
					getField2(o_cards2);
					follow_o2_rev(cards2, copy_cards);
					if (qtyOfCards(cards2) != 0)
					{
						flag++;
						tsuyosa3 = setvalue_pair2(cards2);
					}
				}
			}

			if (state.kumi_info[count][0] == 3)
			{
				tsuyosa = setvalue_kaidan2(temp2);

				cardsDiff3(copy_cards, temp2);

				copyCards(copy_cards2, copy_cards);
				copyCards(copy_cards3, copy_cards);

				follow_o_shibari_rev(o_cards, state.rest_cards);
				follow_o2_rev(o_cards2, state.rest_cards);

				if (state.player_number == 2)
				{
					if (qtyOfCards(o_cards) == qtyOfCards(state.rest_cards))
						flag = 99;
					if (qtyOfCards(o_cards2) == qtyOfCards(state.rest_cards))
						flag = 99;
				}

				if (qtyOfCards(o_cards) == 0)
				{
					flag++;
					tsuyosa2 = STRONG;
				}
				else
				{
					getField2(o_cards);
					follow_o_shibari_rev(cards, copy_cards);
					if (qtyOfCards(cards) != 0)
					{
						flag++;
						kumi_suit = get_suit(cards);
						tsuyosa2 = setvalue_kaidan_shibari(cards, kumi_suit);
					}
				}

				if (qtyOfCards(o_cards2) == 0)
				{
					flag++;
					tsuyosa3 = STRONG;
				}
				else
				{
					getField2(o_cards2);
					follow_o2_rev(cards2, copy_cards);
					if (qtyOfCards(cards2) != 0)
					{
						flag++;
						tsuyosa3 = setvalue_kaidan2(cards2);
					}
				}
			}

			if (flag == 2 && tsuyosa2 >= STRONG && tsuyosa3 >= STRONG)
			{
				cardsDiff3(copy_cards2, cards);
				bunkatsu_pre2(copy_cards2);
				bunkatsu_flag = 1;
			}

			if (flag == 2 && state.kumi_info[13][0] - state.kumi_info[13][h] <= 1 && tsuyosa2 >= STRONG)
			{

				cardsDiff3(copy_cards3, cards2);
				bunkatsu_pre2(copy_cards3);

				if (state.kumi_info[13][0] - state.kumi_info[13][h] <= 1)
				{

					if (tsuyosa > max_tsuyosa)
					{
						copyTable(out_cards, temp2);
						max_tsuyosa = tsuyosa;
					}
				}
			}
		}
		//
		count++;
	}
}

void my_lead14_rev(int out_cards[8][15], int my_cards[8][15])
{ //提出したら負け確定の札を出さないように確認する。残り2人用。（仮）

	int kuminumber = 0;
	int copy_cards[8][15] = {{0}};
	//int copy_cards2[8][15]={{0}};
	//int copy_cards3[8][15]={{0}};

	int o_cards[8][15] = {{0}};
	int o_cards2[8][15] = {{0}};

	int cards[8][15] = {{0}};
	int cards2[8][15] = {{0}};

	int tsuyosa = 0;
	int tsuyosa2 = 0;
	int tsuyosa3 = 0;

	int flag = 0;
	int max_tsuyosa = 0;
	int min_tsuyosa = 1000;
	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};
	int h = 6;

	int kumi_suit = 0;
	int kumi_suitsum = 0;
	int bunkatsu_flag = 0;

	int count = 0;
	int start_kumisuu;

	int rank = 0;
	int rank_s = 0;

	int rest_cards2[8][15] = {{0}};
	int rest_cards3[8][15] = {{0}};
	int temp3[8][15] = {{0}};
	int copy_rest_cards[8][15] = {{0}};
	int copy_submitted_cards[8][15] = {{0}};
	int copy_submitted_cards_plus_hands[8][15] = {{0}};

	copyCards(copy_cards, my_cards);
	bunkatsu_pre2(copy_cards);
	start_kumisuu = state.kumi_info[13][0];
	copyCards(copy_rest_cards, state.rest_cards);
	copyCards(copy_submitted_cards, state.submitted_cards);
	copyCards(copy_submitted_cards_plus_hands, state.submitted_cards_plus_hands);

	while (count < start_kumisuu)
	{

		copyCards(copy_cards, my_cards);
		bunkatsu_pre2(copy_cards);

		if (count == 0)
			copyTable(temp, state.kumi0);
		else if (count == 1)
			copyTable(temp, state.kumi1);
		else if (count == 2)
			copyTable(temp, state.kumi2);
		else if (count == 3)
			copyTable(temp, state.kumi3);
		else if (count == 4)
			copyTable(temp, state.kumi4);
		else if (count == 5)
			copyTable(temp, state.kumi5);
		else if (count == 6)
			copyTable(temp, state.kumi6);
		else if (count == 7)
			copyTable(temp, state.kumi7);
		else if (count == 8)
			copyTable(temp, state.kumi8);
		else if (count == 9)
			copyTable(temp, state.kumi9);
		else if (count == 10)
			copyTable(temp, state.kumi10);

		//
		if (beEmptyCards(temp) == 0)
		{

			copyTable(temp2, temp);
			copyCards(copy_cards, my_cards);
			//if(bunkatsu_flag==1){
			//bunkatsu_pre2(copy_cards);
			//bunkatsu_flag=0;
			//}

			flag = 1; //リセット(?)
			clearTable(o_cards);
			clearTable(o_cards2);
			clearTable(cards);
			clearTable(cards2);
			tsuyosa2 = 0;
			tsuyosa3 = 0;
			copyCards(state.rest_cards, copy_rest_cards);
			copyCards(state.submitted_cards, copy_submitted_cards);
			copyCards(state.submitted_cards_plus_hands, copy_submitted_cards_plus_hands);

			getField2(temp2);

			if (state.kumi_info[count][0] == 1)
			{
				tsuyosa = setvalue_single2(temp2);
				rank = get_rank_min(temp2);
				getField2(temp2);

				cardsDiff3(copy_cards, temp2);
				flag = 0;

				//copyCards(copy_cards2,copy_cards);
				//copyCards(copy_cards3,copy_cards);
				/*
				copyCards(rest_cards2,state,rest_cards);
				copyCards(rest_cards3,state.rest_cards);
				*/
				//follow_o3_shibari(o_cards,state.rest_cards);
				follow_o3_rev(o_cards2, state.rest_cards);
				/*
				if(qtyOfCards(o_cards)==qtyOfCards(state.rest_cards)){//||qtyOfCards5(o_cards,6)>0
					flag=99;
				}
				*/
				if (qtyOfCards(o_cards2) == qtyOfCards(state.rest_cards))
				{
					flag = 99;
				}
				/*	
				if(qtyOfCards(o_cards)==0){
					flag++;
					tsuyosa2=STRONG;
				}else{
					getField2(o_cards);
					follow_o_shibari(cards,copy_cards);
					if(qtyOfCards(cards)!=0){
						flag++;
						kumi_suit=get_suit(cards);
						tsuyosa2=setvalue_single_shibari(cards,kumi_suit);
					}
				}
						
				if(qtyOfCards(o_cards2)==0){
					flag++;
					tsuyosa3=STRONG;
				}else{
					getField2(o_cards2);
					follow_o2(cards2,copy_cards);
					if(qtyOfCards(cards2)!=0){
						flag++;
						tsuyosa3=setvalue_single2(cards2);
					}
				}
				*/
			}

			if (state.kumi_info[count][0] == 2)
			{
				tsuyosa = setvalue_pair2(temp2);
				rank = get_rank_min(temp2);
				getField2(temp2);

				cardsDiff3(copy_cards, temp2);
				flag = 0;

				//copyCards(copy_cards2,copy_cards);
				//copyCards(copy_cards3,copy_cards);
				/*
				copyCards(rest_cards2,state,rest_cards);
				copyCards(rest_cards3,state.rest_cards);
				*/
				//follow_o3_shibari(o_cards,state.rest_cards);
				follow_o3_rev(o_cards2, state.rest_cards);

				if (state.player_number == 2)
				{
					/*
						if(qtyOfCards(o_cards)==qtyOfCards(state.rest_cards)){
							flag=99;
						}
					*/
					if (qtyOfCards(o_cards2) == qtyOfCards(state.rest_cards))
					{
						flag = 99;
					}
				}

				/*
				if(qtyOfCards(o_cards)==0){
					flag++;
					tsuyosa2=STRONG;
				}else{
					getField2(o_cards);
					follow_o_shibari(cards,copy_cards);
					if(qtyOfCards(cards)!=0){
						flag++;
						kumi_suitsum=get_suitsum(cards);
						tsuyosa2=setvalue_pair_shibari(cards,kumi_suitsum);
					}
				}
									
				if(qtyOfCards(o_cards2)==0){
					flag++;
					tsuyosa3=STRONG;
				}else{
					getField2(o_cards2);
					follow_o2(cards2,copy_cards);
					if(qtyOfCards(cards2)!=0){
						flag++;
						tsuyosa3=setvalue_pair2(cards2);
					}
				}
				*/
			}

			if (state.kumi_info[count][0] == 3)
			{
				tsuyosa = setvalue_kaidan2(temp2);
				rank = get_rank_min(temp2);
				getField2(temp2);

				cardsDiff3(copy_cards, temp2);
				flag = 0;

				//copyCards(copy_cards2,copy_cards);
				//copyCards(copy_cards3,copy_cards);
				/*
				copyCards(rest_cards2,state,rest_cards);
				copyCards(rest_cards3,state.rest_cards);
				*/
				//follow_o3_shibari(o_cards,state.rest_cards);
				follow_o3_rev(o_cards2, state.rest_cards);

				if (state.player_number == 2)
				{
					/*
						if(qtyOfCards(o_cards)==qtyOfCards(state.rest_cards)){
							flag=99;
						}
						*/
					if (qtyOfCards(o_cards2) == qtyOfCards(state.rest_cards))
					{
						flag = 99;
					}
				}

				/*
				if(qtyOfCards(o_cards)==0){
					flag++;
					tsuyosa2=STRONG;
				}else{
					getField2(o_cards);
					follow_o_shibari(cards,copy_cards);
					if(qtyOfCards(cards)!=0){
						flag++;
						kumi_suit=get_suit(cards);
						tsuyosa2=setvalue_kaidan_shibari(cards,kumi_suit);
					}
				}
									
				if(qtyOfCards(o_cards2)==0){
					flag++;
					tsuyosa3=STRONG;
				}else{
					getField2(o_cards2);
					follow_o2(cards2,copy_cards);
					if(qtyOfCards(cards2)!=0){
						flag++;
						tsuyosa3=setvalue_kaidan2(cards2);
					}
				}
				*/
			}

			if (flag == 0)
			{
				copyCards(state.rest_cards, copy_cards);
				copyTable5(state.submitted_cards, copy_rest_cards);
				copyCards(state.submitted_cards_plus_hands, state.submitted_cards);
				my_finish_follow2_o(temp3, copy_rest_cards);
				if (qtyOfCards(temp3) != 0)
					flag = 99;
			}
			/*
			if(flag==0){
				if(tsuyosa<min_tsuyosa){
					copyTable(out_cards,temp2);
					min_tsuyosa=tsuyosa;
					
				}
			}
			*/
			if (flag == 0)
			{
				if (rank > rank_s)
				{
					copyTable(out_cards, temp2);
					rank_s = rank;
					//max_tsuyosa=tsuyosa;
				}
			}
		}
		//
		count++;
	}

	copyCards(state.rest_cards, copy_rest_cards);
	copyCards(state.submitted_cards, copy_submitted_cards);
	copyCards(state.submitted_cards_plus_hands, copy_submitted_cards_plus_hands);
}

void my_follow_rev(int out_cards[8][15], int my_cards[8][15])
{
	/*
    他のプレーヤーに続いてカードを出すときのルーチン
    場の状態stateに応じて一枚、枚数組、階段の場合に分けて
    対応すれる関数を呼び出す
    提出するカードはカードテーブルout_cardsに格納される
  */

	clearTable(out_cards);

	bunkatsu_pre2(my_cards);

	if (state.qty == 1)
	{
		//followSolo(out_cards,my_cards,state.joker);    //一枚のとき
		my_followSolo_rev(out_cards, my_cards, state.joker); //一枚のとき
	}
	else
	{
		if (state.sequence == 0)
		{
			my_followGroup_rev(out_cards, my_cards, state.joker); //枚数組のとき
		}
		else
		{
			my_followSequence_rev(out_cards, my_cards, state.joker); //階段のとき
		}
	}
}

void my_followSolo_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int temp[8][15] = {{0}};
	int i = 0, rank = 102, suit = 0, set = 4, tsuyosa2 = 120, tsuyosa3 = 120, kumi_suit = 20, tsuyosa4 = 120;

	if (state.ord == 14 && my_cards[0][1] == 1)
		temp[0][1] = 1; //JOKER単体に対する対応（スペ3返し）

	if (beEmptyCards(temp) == 1)
	{ //JOKERに対する対応以外

		if (state.lock == 1)
		{
			for (i = 0; i < 4; i++)
			{
				if (state.suit[i] == 1)
					suit = i;
			}
		}

		if (state.lock == 1)
		{ //ロックされているなら
			for (i = 0; i < 11; i++)
			{
				if (state.kumi_info[i][2] == suit || state.kumi_info[i][2] == 4)
				{ //マークの判定
					if (state.kumi_info[i][1] == state.qty && state.kumi_info[i][4] < state.ord && state.kumi_info[i][set] >= rank)
					{

						if (i == 0)
							copyTable(temp, state.kumi0);
						else if (i == 1)
							copyTable(temp, state.kumi1);
						else if (i == 2)
							copyTable(temp, state.kumi2);
						else if (i == 3)
							copyTable(temp, state.kumi3);
						else if (i == 4)
							copyTable(temp, state.kumi4);
						else if (i == 5)
							copyTable(temp, state.kumi5);
						else if (i == 6)
							copyTable(temp, state.kumi6);
						else if (i == 7)
							copyTable(temp, state.kumi7);
						else if (i == 8)
							copyTable(temp, state.kumi8);
						else if (i == 9)
							copyTable(temp, state.kumi9);
						else if (i == 10)
							copyTable(temp, state.kumi10);

						rank = state.kumi_info[i][set];
					}
				}
			}
		}
		else
		{ //ロックされていないなら
			for (i = 0; i < 11; i++)
			{
				if (state.kumi_info[i][1] == state.qty && state.kumi_info[i][4] < state.ord && state.kumi_info[i][set] >= rank)
				{

					if (i == 0)
						copyTable(temp, state.kumi0);
					else if (i == 1)
						copyTable(temp, state.kumi1);
					else if (i == 2)
						copyTable(temp, state.kumi2);
					else if (i == 3)
						copyTable(temp, state.kumi3);
					else if (i == 4)
						copyTable(temp, state.kumi4);
					else if (i == 5)
						copyTable(temp, state.kumi5);
					else if (i == 6)
						copyTable(temp, state.kumi6);
					else if (i == 7)
						copyTable(temp, state.kumi7);
					else if (i == 8)
						copyTable(temp, state.kumi8);
					else if (i == 9)
						copyTable(temp, state.kumi9);
					else if (i == 10)
						copyTable(temp, state.kumi10);

					rank = state.kumi_info[i][set];

					//printf("win=1\n");
					//if(state.kumi_info[i][4]==state.kumi_info[13][21])printf("rank \n");
				}
			}
		}
	}

	copyTable(out_cards, temp);
}

void my_followGroup_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{
	/*  
    他のプレーヤーに続いてカードを枚数組で出すときのルーチン
    joker_flagが1の時ジョーカーを使おうとする
    提出するカードはカードテーブルout_cardsに格納される
  
  int group[8][15];
  int ngroup[8][15];
  int temp[8][15];
  
  highCards(temp,my_cards,state.ord);          //場より強いカードを残す 
  if(state.lock==1){                           //ロックされているとき
    lockCards(temp,state.suit);                //出せないカードを除去
  }
  makeGroupTable(group,temp);                  //残ったものから枚数組を書き出す
  if(nCards(ngroup,group,state.qty)==0&&state.joker==1){
    //場と同じ枚数の組が無いときジョーカーを使って探す
    makeJGroupTable(group,temp);               
    nCards(ngroup,group,state.qty);     //場と同じ枚数の組のみのこす。 
  }
  lowGroup(out_cards,my_cards,ngroup);  //一番弱い組を抜き出す
	*/

	int temp[8][15] = {{0}};
	int i = 0, rank = 102, suitsum = 0, set = 4;

	if (state.lock == 1)
	{
		for (i = 0; i < 4; i++)
		{
			if (state.suit[i] == 1) //マークを1,2,4,8の組み合わせで表すことにした。
				if (i == 0)
					suitsum = suitsum + 1;
			if (i == 1)
				suitsum = suitsum + 2;
			if (i == 2)
				suitsum = suitsum + 4;
			if (i == 3)
				suitsum = suitsum + 8;
		}
	}

	if (state.lock == 1)
	{ //ロックされているなら
		for (i = 0; i < 11; i++)
		{
			if (state.kumi_info[i][2] == suitsum)
			{ //マークの判定
				if (state.kumi_info[i][1] == state.qty && state.kumi_info[i][4] < state.ord && state.kumi_info[i][set] >= rank)
				{

					if (i == 0)
						copyTable(temp, state.kumi0);
					else if (i == 1)
						copyTable(temp, state.kumi1);
					else if (i == 2)
						copyTable(temp, state.kumi2);
					else if (i == 3)
						copyTable(temp, state.kumi3);
					else if (i == 4)
						copyTable(temp, state.kumi4);
					else if (i == 5)
						copyTable(temp, state.kumi5);
					else if (i == 6)
						copyTable(temp, state.kumi6);
					else if (i == 7)
						copyTable(temp, state.kumi7);
					else if (i == 8)
						copyTable(temp, state.kumi8);
					else if (i == 9)
						copyTable(temp, state.kumi9);
					else if (i == 10)
						copyTable(temp, state.kumi10);

					rank = state.kumi_info[i][set];
				}
			}
		}
	}
	else
	{ //ロックされていないなら
		for (i = 0; i < 11; i++)
		{
			if (state.kumi_info[i][1] == state.qty && state.kumi_info[i][4] < state.ord && state.kumi_info[i][set] >= rank)
			{

				if (i == 0)
					copyTable(temp, state.kumi0);
				else if (i == 1)
					copyTable(temp, state.kumi1);
				else if (i == 2)
					copyTable(temp, state.kumi2);
				else if (i == 3)
					copyTable(temp, state.kumi3);
				else if (i == 4)
					copyTable(temp, state.kumi4);
				else if (i == 5)
					copyTable(temp, state.kumi5);
				else if (i == 6)
					copyTable(temp, state.kumi6);
				else if (i == 7)
					copyTable(temp, state.kumi7);
				else if (i == 8)
					copyTable(temp, state.kumi8);
				else if (i == 9)
					copyTable(temp, state.kumi9);
				else if (i == 10)
					copyTable(temp, state.kumi10);

				rank = state.kumi_info[i][set];
			}
		}
	}

	copyTable(out_cards, temp);
}

void my_followSequence_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{
	/*
    他のプレーヤーに続いてカードを階段で出すときのルーチン
    joker_flagが1の時ジョーカーを使おうとする
    提出するカードはカードテーブルout_cardsに格納される
  
  int seq[8][15];
  int nseq[8][15];
  int temp[8][15];
  
  highCards(temp,my_cards,state.ord);          //場より強いカードを残す
  if(state.lock==1){                           //ロックされているとき
    lockCards(temp,state.suit);                //出せないカードを除去
  }
  makeKaidanTable(seq,temp);                   //階段を書き出す
  if(nCards(nseq,seq,state.qty)==0&&state.joker==1){
    //場と同じ枚数の階段が無いときジョーカーを使って探す
    makeJKaidanTable(seq,temp);
    nCards(nseq,seq,state.qty);          //場と同じ枚数の組のみのこす。
  }
  lowSequence(out_cards,my_cards,nseq);  //一番弱い階段を
	
	*/
	int temp[8][15] = {{0}};
	int i = 0, rank = 102, suit = 0, set = 4;

	if (state.lock == 1)
	{
		for (i = 0; i < 4; i++)
		{
			if (state.suit[i] == 1)
				suit = i;
		}
	}

	if (state.lock == 1)
	{ //ロックされているなら
		for (i = 0; i < 11; i++)
		{
			if (state.kumi_info[i][2] == suit || state.kumi_info[i][2] == 4)
			{ //マークの判定
				if (state.kumi_info[i][1] == state.qty && state.kumi_info[i][4] < state.ord && state.kumi_info[i][set] >= rank)
				{

					if (i == 0)
						copyTable(temp, state.kumi0);
					else if (i == 1)
						copyTable(temp, state.kumi1);
					else if (i == 2)
						copyTable(temp, state.kumi2);
					else if (i == 3)
						copyTable(temp, state.kumi3);
					else if (i == 4)
						copyTable(temp, state.kumi4);
					else if (i == 5)
						copyTable(temp, state.kumi5);
					else if (i == 6)
						copyTable(temp, state.kumi6);
					else if (i == 7)
						copyTable(temp, state.kumi7);
					else if (i == 8)
						copyTable(temp, state.kumi8);
					else if (i == 9)
						copyTable(temp, state.kumi9);
					else if (i == 10)
						copyTable(temp, state.kumi10);

					rank = state.kumi_info[i][set];
				}
			}
		}
	}
	else
	{ //ロックされていないなら
		for (i = 0; i < 11; i++)
		{
			if (state.kumi_info[i][1] == state.qty && state.kumi_info[i][4] < state.ord && state.kumi_info[i][set] >= rank)
			{

				if (i == 0)
					copyTable(temp, state.kumi0);
				else if (i == 1)
					copyTable(temp, state.kumi1);
				else if (i == 2)
					copyTable(temp, state.kumi2);
				else if (i == 3)
					copyTable(temp, state.kumi3);
				else if (i == 4)
					copyTable(temp, state.kumi4);
				else if (i == 5)
					copyTable(temp, state.kumi5);
				else if (i == 6)
					copyTable(temp, state.kumi6);
				else if (i == 7)
					copyTable(temp, state.kumi7);
				else if (i == 8)
					copyTable(temp, state.kumi8);
				else if (i == 9)
					copyTable(temp, state.kumi9);
				else if (i == 10)
					copyTable(temp, state.kumi10);

				rank = state.kumi_info[i][set];
			}
		}
	}

	copyTable(out_cards, temp);
}
void my_follow3_rev(int out_cards[8][15], int my_cards[8][15])
{

	clearTable(out_cards);
	state.tsuyosa = 0;
	if (state.qty == 1)
	{
		my_followSolo3_rev(out_cards, my_cards, state.joker); //一枚のとき
	}
	else
	{
		if (state.sequence == 0)
		{
			my_followGroup3_rev(out_cards, my_cards, state.joker); //枚数組のとき
		}
		else
		{
			my_followSequence3_rev(out_cards, my_cards, state.joker); //階段のとき
		}
	}
}

void my_followSolo3_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};
	int temp8[8][15] = {{0}};
	int copy_cards[8][15] = {{0}};

	int tsuyosa = 0, ba_suit = 0, kumi_suit = 0;

	int handvalue = 0;

	int h = 5;
	int nagashi_kumi = 0, nagashi_kumi2 = 0, nagashi_flag = 0;
	int kumisuu = 0, kumisuu2 = 0;

	int rank = 0, flag = 0;
	int tsuyosa2 = 0;
	int kakunin = 0;

	int hh = 16, x = 0, y = 0;

	int rest_rank_min = 0;

	int shibari_sa = 0;
	int start_handvalue = 0;
	int handvalue2 = 0;

	int flag2 = 0;
	int kumi_suit_s = 9;
	int flag3 = 0;

	int kumi_rank = 0;
	int kumi_rank_s = 0;

	int sonomark = 99;

	int flag4 = 0;

	rest_rank_min = get_rank_min(state.rest_cards);

	clearTable(out_cards);
	copyCards(copy_cards, my_cards);
	bunkatsu_pre2(my_cards);

	if (state.player_number >= 4)
		handvalue = state.kumi_info[13][h] - 1;
	if (state.player_number == 3)
		handvalue = state.kumi_info[13][h] - 1;
	if (state.player_number == 2)
		handvalue = 0;

	start_handvalue = handvalue;

	nagashi_kumi = state.kumi_info[13][35];
	kumisuu = state.kumi_info[13][0];

	if (state.suit[0] == 1)
		ba_suit = 0;
	if (state.suit[1] == 1)
		ba_suit = 1;
	if (state.suit[2] == 1)
		ba_suit = 2;
	if (state.suit[3] == 1)
		ba_suit = 3;

	lowCards(temp, copy_cards, state.ord);

	if (state.lock == 1)
	{
		lockCards(temp, state.suit);
	}

	while (qtyOfCards(temp) > 0)
	{
		copyCards(copy_cards, my_cards);

		shibari_sa = 0;

		if (state.find_s3 == 0)
			joker_delete(temp);

		//if(qtyOfCards5(my_cards,6)>=2){
		//	highSolo2_1(temp2,temp,state.joker);
		//}else{
		highSolo2(temp2, temp, state.joker);
		//}

		kumi_suit = get_suit(temp2);
		kumi_rank = get_rank_min(temp2);

		if (kumi_suit == ba_suit)
		{
			rank = get_rank_max(temp2);
			kakunin = qtyOfCards11(my_cards, kumi_suit);

			flag = 0;
			flag2 = 0;
			flag4 = 0;

			tsuyosa = setvalue_single_shibari(temp2, kumi_suit);
			tsuyosa2 = setvalue_single2(temp2);

			if (kakunin != 0)
			{ //&&qtyOfCards6(state.submitted_cards_plus_hands,kumi_suit,6)==1//&&kakunin!=1//&&kakunin!=rank
				//&&kakunin!=rank
				/*
				if(kakunin==1){
					clearTable(temp8);
					highCards2(temp8,copy_cards,kumi_rank,kumi_suit);
					bunkatsu_pre2(temp8);
					if(state.kumi_info[13][0]-state.kumi_info[13][35]<=2||kumi_rank_s==0||kumi_rank>=kumi_rank_s-6)flag2=1;
				}
				
				if(kakunin!=1){
					clearTable(temp8);
					highCards2(temp8,copy_cards,kumi_rank,kumi_suit);
					bunkatsu_pre2(temp8);
					if(state.kumi_info[13][0]-state.kumi_info[13][35]<=2||kumi_rank_s==0||kumi_rank>=kumi_rank_s-7)flag2=1;
				}
				*/
				flag2 = 1;

				if (qtyOfCards7(state.rest_cards, kakunin) > 0)
					flag4 = 1;

				if (flag2 == 1)
				{

					if (state.player_number > 2)
					{
						if (tsuyosa2 < tsuyosa)
						{
							if (kumi_rank >= kumi_rank_s - 0 || kumi_rank_s == 0 || (flag4 == 1 && kumi_rank >= kumi_rank_s - 4))
							{ //||flag4==1
								tsuyosa = tsuyosa2 + 1;
							}
							else
							{
								tsuyosa = tsuyosa2;
							}
						}
						else
						{
							tsuyosa = tsuyosa2;
						}
					}
					else
					{
						tsuyosa = tsuyosa2;
					}

					if (tsuyosa > STRONG)
						tsuyosa = STRONG;

					//if(state.player_number>2)shibari_flag=1;//

					flag = 1;

					shibari_sa = tsuyosa - tsuyosa2;
				}

				/*
				if(flag==0){
					if(tsuyosa==STRONG){
						if(tsuyosa2<tsuyosa){
							if(kumi_rank_s==0||flag3==0){
								if(kumi_rank>=kumi_rank_s-2||kumi_rank_s==0){
									tsuyosa=tsuyosa2+1;
								}else{
									tsuyosa=tsuyosa2;
								}
							}else{
								tsuyosa=tsuyosa2;
							}
						}else{
							tsuyosa=tsuyosa2;
						}
						shibari_sa=tsuyosa-tsuyosa2;
					}else{
						tsuyosa=tsuyosa2;
					}
				}
				*/
			}
			else
			{
				tsuyosa = tsuyosa2;
			}
		}
		else
		{
			tsuyosa = setvalue_single2(temp2);
		}

		cardsDiff3(temp, temp2);
		cardsDiff3(copy_cards, temp2);
		bunkatsu_pre2(copy_cards);
		if (tsuyosa >= STRONG - 1)
		{
			nagashi_flag = 1;
		}
		else
		{
			nagashi_flag = 0;
		}
		nagashi_kumi2 = state.kumi_info[13][35];
		kumisuu2 = state.kumi_info[13][0];
		/*
		printf("handvalue   %d\n",handvalue);
		printf("tsuyosa      %d\n",tsuyosa);
		outputCards(temp2);
		*/
		//y=(state.kumi_info[13][hh]+tsuyosa)/(state.kumi_info[13][0]+1);

		if (state.player_number > 2)
		{
			/*
			if(tsuyosa+state.kumi_info[13][h]==handvalue&&y>=x&&nagashi_kumi2+nagashi_flag>=nagashi_kumi&&kumi_rank==kumi_rank_s&&(kumi_suit==qtyOfCards15(state.submitted_cards,kumi_rank)||qtyOfCards13(state.rest_cards,kumi_rank,kumi_suit)<2)&&(kumi_rank==6||kumi_rank==1)){//&&kumisuu2-nagashi_kumi2<=kumisuu-nagashi_kumi//&&nagashi_kumi2+nagashi_flag>=nagashi_kumi
				//&&nagashi_kumi2+nagashi_flag>=nagashi_kumi
				
				handvalue=tsuyosa+state.kumi_info[13][h];
				copyTable(out_cards,temp2);
				state.tsuyosa=tsuyosa;
				x=y;
				kumi_rank_s=kumi_rank;
				kumi_suit_s=kumi_suit;
				flag3=flag2;
				//sonomark=qtyOfCards13(state.rest_cards,kumi_rank,kumi_suit);
			}
			*/
			//&&y>=x
			if (tsuyosa + state.kumi_info[13][h] > handvalue && nagashi_kumi2 + nagashi_flag >= nagashi_kumi)
			{ //&&kumisuu2-nagashi_kumi2<=kumisuu-nagashi_kumi//&&nagashi_kumi2+nagashi_flag>=nagashi_kumi
				//&&nagashi_kumi2+nagashi_flag>=nagashi_kumi

				handvalue = tsuyosa + state.kumi_info[13][h];
				copyTable(out_cards, temp2);
				state.tsuyosa = tsuyosa;
				x = y;
				kumi_rank_s = kumi_rank;
				kumi_suit_s = kumi_suit;
				flag3 = flag2;
			}
		}
		else
		{
			if (tsuyosa + state.kumi_info[13][h] > handvalue)
			{ //&&kumisuu2-nagashi_kumi2<=kumisuu-nagashi_kumi

				handvalue = tsuyosa + state.kumi_info[13][h];
				copyTable(out_cards, temp2);
				state.tsuyosa = tsuyosa;
				kumi_rank_s = kumi_rank;
				kumi_suit_s = kumi_suit;
				flag3 = flag2;
			}
		}
	}
}

void my_followGroup3_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int group[8][15];
	int ngroup[8][15];

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};
	int copy_cards[8][15] = {{0}};

	int tsuyosa = 0, ba_suitsum = 0, kumi_suitsum = 0, j = 0;
	int handvalue = 0;

	int h = 5;
	int rank2 = 0, pattern = 0;

	int nagashi_kumi = 0, nagashi_kumi2 = 0, nagashi_flag = 0;

	int tsuyosa2 = 0;
	int hh = 16, x = 0, y = 0;

	int flag = 0;
	int kumi_rank = 0;
	int kumi_rank_s = 0;

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);
	if (state.player_number > 2)
	{
		handvalue = state.kumi_info[13][h] - 1;
	}
	else
	{
		//handvalue=state.kumi_info[13][h]-10;
		handvalue = 0;
	}
	nagashi_kumi = state.kumi_info[13][35];

	copyTable(copy_cards, my_cards);

	if (state.suit[0] == 1)
		ba_suitsum = ba_suitsum + 1;
	if (state.suit[1] == 1)
		ba_suitsum = ba_suitsum + 2;
	if (state.suit[2] == 1)
		ba_suitsum = ba_suitsum + 4;
	if (state.suit[3] == 1)
		ba_suitsum = ba_suitsum + 8;

	//まず標準プログラムのfollowのように提出手を求める。
	lowCards(temp, copy_cards, state.ord); //場より強いカードを残す
	if (state.lock == 1)
	{								 //ロックされているとき
		lockCards(temp, state.suit); //出せないカードを除去
	}
	//makeGroupTable(group,temp);                  //残ったものから枚数組を書き出す
	makeJGroupTable2(group, temp);
	nCards2(ngroup, group, state.qty);

	if (qtyOfCards(ngroup) >= state.qty)
	{

		for (rank2 = state.ord - 1; rank2 > 0; rank2--)
		{
			for (pattern = 0; pattern < 6; pattern++)
			{

				copyCards(copy_cards, my_cards);

				Group2(temp2, copy_cards, ngroup, rank2, state.qty, pattern);

				if (qtyOfCards(temp2) == state.qty)
				{

					kumi_rank = get_rank_min(temp2);

					kumi_suitsum = get_suitsum(temp2);
					if (kumi_suitsum == ba_suitsum)
					{
						tsuyosa = setvalue_pair_shibari3(temp2, kumi_suitsum);
						tsuyosa2 = setvalue_pair3(temp2);

						if (state.player_number > 2)
						{
							/*
								if(tsuyosa<STRONG*state.qty&&tsuyosa2!=tsuyosa&&flag==0){//&&kumi_rank_s>=kumi_rank-5
									tsuyosa=tsuyosa2+state.qty-1;
									flag=1;
								}
								*/

							if (tsuyosa == STRONG * state.qty)
							{
								if (tsuyosa2 < tsuyosa)
								{
									if (kumi_rank >= kumi_rank_s - 2 || kumi_rank_s == 0)
									{
										tsuyosa = tsuyosa2 + 1;
									}
									else
									{
										tsuyosa = tsuyosa2;
									}
								}
								else
								{
									tsuyosa = tsuyosa2;
								}
							}
							else
							{
								tsuyosa = tsuyosa2;
							}
						}
						else
						{
							if (tsuyosa < STRONG * state.qty && tsuyosa2 != tsuyosa)
								tsuyosa = tsuyosa2;
						}

						if (tsuyosa > STRONG * state.qty)
							tsuyosa = STRONG * state.qty;
					}
					else
					{
						tsuyosa = setvalue_pair3(temp2);
					}

					cardsDiff3(copy_cards, temp2);
					bunkatsu_pre2(copy_cards);
					if (tsuyosa >= (STRONG - 1) * state.qty)
					{
						nagashi_flag = 1;
					}
					else
					{
						nagashi_flag = 0;
					}
					nagashi_kumi2 = state.kumi_info[13][35];

					//y=(state.kumi_info[13][hh]+(tsuyosa/state.qty))/(state.kumi_info[13][0]+1);

					if (state.player_number > 2)
					{
						/*
							if(tsuyosa+state.kumi_info[13][h]==handvalue&&nagashi_kumi2+nagashi_flag>=nagashi_kumi&&y>=x&&kumi_rank==kumi_rank_s&&kumi_suitsum==ba_suitsum){
								handvalue=tsuyosa+state.kumi_info[13][h];
								copyTable(out_cards,temp2);
								state.tsuyosa=tsuyosa;
								x=y;
								kumi_rank_s=kumi_rank;
							}
							*/
						if (tsuyosa + state.kumi_info[13][h] > handvalue && nagashi_kumi2 + nagashi_flag >= nagashi_kumi)
						{ //&&y>=x
							handvalue = tsuyosa + state.kumi_info[13][h];
							copyTable(out_cards, temp2);
							state.tsuyosa = tsuyosa;
							x = y;
							kumi_rank_s = kumi_rank;
						}
					}
					else
					{
						if (tsuyosa + state.kumi_info[13][h] > handvalue)
						{
							handvalue = tsuyosa + state.kumi_info[13][h];
							copyTable(out_cards, temp2);
							state.tsuyosa = tsuyosa;
							kumi_rank_s = kumi_rank;
						}
					}
				}
			}
		}
	}
}

void my_followSequence3_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int group[8][15];
	int ngroup[8][15];

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};
	int copy_cards[8][15] = {{0}};

	int tsuyosa = 0, ba_suit = 0, kumi_suit = 0, j = 0;
	int handvalue = 0;

	int h = 5;
	int rank2 = 0, pattern = 0;
	int nagashi_kumi = 0, nagashi_kumi2 = 0, nagashi_flag = 0;
	int kumisuu = 0, kumisuu2 = 0;
	int tsuyosa2 = 0;
	int hh = 16, x = 0, y = 0;

	int kumi_rank = 0;
	int kumi_rank_s = 0;
	int shibari_flag = 0;

	int flag_sh = 0;

	int rest_rank_max = 0;
	int flag = 0;

	int mark = 0;

	rest_rank_max = get_rank_max(state.rest_cards);

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);
	if (state.player_number > 2)
	{
		handvalue = state.kumi_info[13][h] - 1;
	}
	else
	{
		handvalue = 0;
	}

	nagashi_kumi = state.kumi_info[13][7];
	kumisuu = state.kumi_info[13][0];

	copyTable(copy_cards, my_cards);

	if (state.suit[0] == 1)
		ba_suit = 0;
	if (state.suit[1] == 1)
		ba_suit = 1;
	if (state.suit[2] == 1)
		ba_suit = 2;
	if (state.suit[3] == 1)
		ba_suit = 3;

	//まず標準プログラムのfollowのように提出手を求める。
	lowCards(temp, copy_cards, state.ord); //場より強いカードを残す
	if (state.lock == 1)
	{								 //ロックされているとき
		lockCards(temp, state.suit); //出せないカードを除去
	}

	if (qtyOfCards(temp) >= state.qty)
	{

		for (pattern = 11; pattern >= 0; pattern--)
		{
			for (mark = 0; mark < 4; mark++)
			{

				copyCards(copy_cards, my_cards);

				Sequence2(temp2, temp, mark, state.qty, pattern);

				if (qtyOfCards(temp2) == state.qty && get_rank_max(temp2) < state.ord)
				{

					kumi_rank = 0;
					shibari_flag = 0;
					flag = 0;

					kumi_suit = get_suit(temp2);
					kumi_rank = get_rank_max(temp2);
					if (kumi_suit == ba_suit)
					{
						tsuyosa = setvalue_kaidan_shibari3(temp2, kumi_suit);
						tsuyosa2 = setvalue_kaidan3(temp2);
						if (state.player_number > 2)
						{
							/*
							if(tsuyosa<STRONG*state.qty&&tsuyosa2!=tsuyosa&&flag==0){
								tsuyosa=tsuyosa2+state.qty-1;
								flag=1;
							}
							*/

							if (tsuyosa == STRONG * state.qty)
							{
								if (tsuyosa2 < tsuyosa)
								{
									//tsuyosa=tsuyosa2+2;
									tsuyosa = tsuyosa - 1;
								}
								else
								{
									tsuyosa = tsuyosa2;
								}
							}
							else
							{
								tsuyosa = tsuyosa2;
							}

							shibari_flag = 1;
						}
						else
						{
							if (tsuyosa < STRONG * state.qty && tsuyosa2 != tsuyosa)
								tsuyosa = tsuyosa2;
						}
						if (tsuyosa > STRONG * state.qty)
							tsuyosa = STRONG * state.qty;
					}
					else
					{
						tsuyosa = setvalue_kaidan3(temp2);
					}

					cardsDiff3(copy_cards, temp2);
					bunkatsu_pre2(copy_cards);

					if (tsuyosa >= (STRONG - 1) * state.qty)
					{
						nagashi_flag = 1;
					}
					else
					{
						nagashi_flag = 0;
					}
					nagashi_kumi2 = state.kumi_info[13][35];
					kumisuu2 = state.kumi_info[13][0];

					//y=(state.kumi_info[13][hh]+(tsuyosa/state.qty))/(state.kumi_info[13][0]+1);

					if (state.player_number > 2)
					{
						/*
					if(tsuyosa+state.kumi_info[13][h]==handvalue&&nagashi_kumi2+nagashi_flag>=nagashi_kumi&&y>=x&&kumi_rank==kumi_rank_s&&kumi_suit==ba_suit){//&&nagashi_kumi2+1>=nagashi_kumi&&kumisuu2-nagashi_kumi2<=kumisuu-nagashi_kumi
						
						handvalue=tsuyosa+state.kumi_info[13][h];
						copyTable(out_cards,temp2);
						state.tsuyosa=tsuyosa;
						x=y;
						kumi_rank_s=kumi_rank;

					}
					*/
						//&&y>=x
						if (tsuyosa + state.kumi_info[13][h] > handvalue && nagashi_kumi2 + nagashi_flag >= nagashi_kumi)
						{ //&&nagashi_kumi2+1>=nagashi_kumi&&kumisuu2-nagashi_kumi2<=kumisuu-nagashi_kumi

							handvalue = tsuyosa + state.kumi_info[13][h];
							copyTable(out_cards, temp2);
							state.tsuyosa = tsuyosa;
							x = y;
							kumi_rank_s = kumi_rank;
						}
					}
					else
					{
						if (tsuyosa + state.kumi_info[13][h] > handvalue)
						{ //&&nagashi_kumi2+1>=nagashi_kumi&&kumisuu2-nagashi_kumi2<=kumisuu-nagashi_kumi

							handvalue = tsuyosa + state.kumi_info[13][h];
							copyTable(out_cards, temp2);
							state.tsuyosa = tsuyosa;
						}
					}
				}
			}
		}
	}
}

/*
void my_followSequence3_rev(int out_cards[8][15],int my_cards[8][15],int joker_flag){

  int seq[8][15];
  int nseq[8][15];
 
  int temp[8][15]={{0}}; 
  int temp2[8][15]={{0}};
  int copy_cards[8][15]={{0}};
  int copy_cards2[8][15]={{0}};
  int copy_cards3[8][15]={{0}};
  int copy_cards4[8][15]={{0}};
	
  int tsuyosa=0,ba_suit=0,kumi_suit=0,j=0;
  int handvalue=0;
	
  int h=5;
  int nagashi_kumi=0,nagashi_kumi2=0,nagashi_flag=0;
	
  clearTable(out_cards);
  bunkatsu_pre2(my_cards);
 	if(state.player_number>2){
		handvalue=state.kumi_info[13][h]-1;
	}else{
		//handvalue=state.kumi_info[13][h]-10;
		handvalue=0;
	}
  nagashi_kumi=state.kumi_info[13][35];

  copyTable(copy_cards,my_cards);
  copyTable(copy_cards2,my_cards);
  copyTable(copy_cards3,my_cards);
  copyTable(copy_cards4,my_cards);
	
  if(state.suit[0]==1)ba_suit=0;
  if(state.suit[1]==1)ba_suit=1;
  if(state.suit[2]==1)ba_suit=2;
  if(state.suit[3]==1)ba_suit=3;
	
 //まず標準プログラムのfollowのように提出手を求める。
  lowCards(temp,my_cards,state.ord);          //場より強いカードを残す
  if(state.lock==1){                           //ロックされているとき
    lockCards(temp,state.suit);                //出せないカードを除去
  }
  makeKaidanTable(seq,temp);                   //階段を書き出す
  nCards2(nseq,seq,state.qty);
  
  		highSequence(temp2,copy_cards,nseq);//弱いほうから1組
		 if(qtyOfCards(temp2)==state.qty){
		 	kumi_suit=get_suit(temp2);
			if(kumi_suit==ba_suit){
				tsuyosa=setvalue_kaidan_shibari3(temp2,kumi_suit);
				if(tsuyosa<STRONG*state.qty)tsuyosa=setvalue_kaidan3(temp2);
			}else{
				tsuyosa=setvalue_kaidan3(temp2);
			}
		 	
			cardsDiff3(copy_cards,temp2);
		 	bunkatsu_pre2(copy_cards);
		 	if(tsuyosa>=(STRONG-1)*state.qty){
					nagashi_flag=1;
				}else{
					nagashi_flag=0;
				}
			nagashi_kumi2=state.kumi_info[13][35];
			
		 	if(state.player_number>2){
					if(tsuyosa+state.kumi_info[13][h]>handvalue&&nagashi_kumi2+nagashi_flag>=nagashi_kumi){//&&nagashi_kumi2+1>=nagashi_kumi
					 	
							handvalue=tsuyosa+state.kumi_info[13][h];
							copyTable(out_cards,temp2);
					 		state.tsuyosa=tsuyosa;
					}
				
				}else{
				 	if(tsuyosa+state.kumi_info[13][h]>handvalue){//&&nagashi_kumi2+1>=nagashi_kumi
					 		
							handvalue=tsuyosa+state.kumi_info[13][h];
							copyTable(out_cards,temp2);
					 		state.tsuyosa=tsuyosa;
					 		
					}
			}
		 }
	
		clearTable(temp2);
		lowSequence(temp2,copy_cards2,nseq);//強いほうから1組
		if(qtyOfCards(temp2)==state.qty){
		 	kumi_suit=get_suit(temp2);
			if(kumi_suit==ba_suit){
				tsuyosa=setvalue_kaidan_shibari3(temp2,kumi_suit);
				if(tsuyosa<STRONG*state.qty)tsuyosa=setvalue_kaidan3(temp2);
			}else{
				tsuyosa=setvalue_kaidan3(temp2);
			}
		 	
			cardsDiff3(copy_cards2,temp2);
		 	bunkatsu_pre2(copy_cards2);
			if(tsuyosa>=(STRONG-1)*state.qty){
					nagashi_flag=1;
				}else{
					nagashi_flag=0;
				}
			nagashi_kumi2=state.kumi_info[13][35];
			
		 	if(state.player_number>2){
					if(tsuyosa+state.kumi_info[13][h]>handvalue&&nagashi_kumi2+nagashi_flag>=nagashi_kumi){//&&nagashi_kumi2+1>=nagashi_kumi
					 	
							handvalue=tsuyosa+state.kumi_info[13][h];
							copyTable(out_cards,temp2);
					 		state.tsuyosa=tsuyosa;
					}
				
				}else{
				 	if(tsuyosa+state.kumi_info[13][h]>handvalue){//&&nagashi_kumi2+1>=nagashi_kumi
					 		
							handvalue=tsuyosa+state.kumi_info[13][h];
							copyTable(out_cards,temp2);
					 		state.tsuyosa=tsuyosa;
					 		
					}
			}
		}
	
		if(state.joker==1){
    		//ジョーカーを使って探す
	   	 	makeJKaidanTable(seq,temp);
	   		nCards2(nseq,seq,state.qty); 
			
			clearTable(temp2);
			highSequence(temp2,copy_cards3,nseq);//弱いほうから1組
			 if(qtyOfCards(temp2)==state.qty){
			 	kumi_suit=get_suit(temp2);
				if(kumi_suit==ba_suit){
					tsuyosa=setvalue_kaidan_shibari3(temp2,kumi_suit);
					if(tsuyosa<STRONG*state.qty)tsuyosa=setvalue_kaidan3(temp2);
				}else{
					tsuyosa=setvalue_kaidan3(temp2);
				}
			 	
				cardsDiff3(copy_cards3,temp2);
			 	bunkatsu_pre2(copy_cards3);
			 	if(tsuyosa>=(STRONG-1)*state.qty){
					nagashi_flag=1;
				}else{
					nagashi_flag=0;
				}
			nagashi_kumi2=state.kumi_info[13][35];
				
			 	if(state.player_number>2){
					if(tsuyosa+state.kumi_info[13][h]>handvalue&&nagashi_kumi2+nagashi_flag>=nagashi_kumi){//&&nagashi_kumi2+1>=nagashi_kumi
					 	
							handvalue=tsuyosa+state.kumi_info[13][h];
							copyTable(out_cards,temp2);
					 		state.tsuyosa=tsuyosa;
					}
				
				}else{
				 	if(tsuyosa+state.kumi_info[13][h]>handvalue){//&&nagashi_kumi2+1>=nagashi_kumi
					 		
							handvalue=tsuyosa+state.kumi_info[13][h];
							copyTable(out_cards,temp2);
					 		state.tsuyosa=tsuyosa;
					 		
					}
				}
			 	
		 	}
	
			clearTable(temp2);
			lowSequence(temp2,copy_cards4,nseq);//強いほうから1組
			if(qtyOfCards(temp2)==state.qty){
			 	kumi_suit=get_suit(temp2);
				if(kumi_suit==ba_suit){
					tsuyosa=setvalue_kaidan_shibari3(temp2,kumi_suit);
					if(tsuyosa<STRONG*state.qty)tsuyosa=setvalue_kaidan3(temp2);
				}else{
					tsuyosa=setvalue_kaidan3(temp2);
				}
			 	
				cardsDiff3(copy_cards4,temp2);
			 	bunkatsu_pre2(copy_cards4);
				if(tsuyosa>=(STRONG-1)*state.qty){
					nagashi_flag=1;
				}else{
					nagashi_flag=0;
				}
			nagashi_kumi2=state.kumi_info[13][35];
				
			 	if(state.player_number>2){
					if(tsuyosa+state.kumi_info[13][h]>handvalue&&nagashi_kumi2+nagashi_flag>=nagashi_kumi){//&&nagashi_kumi2+1>=nagashi_kumi
					 	
							handvalue=tsuyosa+state.kumi_info[13][h];
							copyTable(out_cards,temp2);
					 		state.tsuyosa=tsuyosa;
					}
				
				}else{
				 	if(tsuyosa+state.kumi_info[13][h]>handvalue){//&&nagashi_kumi2+1>=nagashi_kumi
					 		
							handvalue=tsuyosa+state.kumi_info[13][h];
							copyTable(out_cards,temp2);
					 		state.tsuyosa=tsuyosa;
					 		
					}
				}
			}
				
		}
	
}
*/

void my_follow_s3(int out_cards[8][15], int my_cards[8][15])
{ //自分が提出したjokerに対して

	int copy_cards[8][15] = {{0}};
	int value1 = 0, value2 = 0;

	int h = 0;
	int hako = 0;

	if (state.rev == 0)
	{
		h = 6;
	}
	else
	{
		h = 7;
	}

	copyTable(copy_cards, my_cards);

	bunkatsu_pre2(copy_cards);
	value1 = state.kumi_info[13][17];
	hako = state.kumi_info[13][0] - state.kumi_info[13][h];

	copy_cards[0][1] = 0;
	bunkatsu_pre2(copy_cards);
	value2 = state.kumi_info[13][17];

	if (state.kumi_info[13][0] - state.kumi_info[13][h] < hako)
		out_cards[0][1] = 1;
	if (state.kumi_info[13][0] - state.kumi_info[13][h] <= 1)
		out_cards[0][1] = 1;
}

void my_follow_s3_2(int out_cards[8][15], int my_cards[8][15])
{ //自分以外が提出したjokerに対して

	int copy_cards[8][15] = {{0}};
	int value1 = 0, value2 = 0;

	int h = 0;
	int hako = 0;

	if (state.rev == 0)
	{
		h = 6;
	}
	else
	{
		h = 7;
	}

	copyTable(copy_cards, my_cards);

	bunkatsu_pre2(copy_cards);
	value1 = state.kumi_info[13][17];
	hako = state.kumi_info[13][0] - state.kumi_info[13][h];

	copy_cards[0][1] = 0;
	bunkatsu_pre2(copy_cards);
	value2 = state.kumi_info[13][17];

	if (state.kumi_info[13][0] - state.kumi_info[13][h] <= hako)
		out_cards[0][1] = 1;
	if (state.kumi_info[13][0] - state.kumi_info[13][h] <= 1)
		out_cards[0][1] = 1;

	//if(value1<=value2||state.kumi_info[13][0]-state.kumi_info[13][h]<=1)out_cards[0][1]=1;
}

void my_finish_lead_rev(int out_cards[8][15], int my_cards[8][15])
{

	int kuminumber = 0, set = 1; //[1]に組の枚数が収納されている

	int saizyaku_kumi = 0;
	int saizyaku_rank = 0;

	clearTable(out_cards);

	state.finish = 1;
	bunkatsu_pre2(my_cards);
	state.finish = 0;

	if (state.kumi_info[13][0] - state.kumi_info[13][7] <= 1)
	{ //「手札の組数」-「流せると判断された組数」<=1

		saizyaku_kumi = min_kumi_info(12);
		saizyaku_rank = state.kumi_info[saizyaku_kumi][4];
		/*
	   	 kuminumber=min_kumi_info(10);//1枚あたりの流し評価値が最も小さいものを見つける。
		
		if(kuminumber==0)state.kumi_info[0][set]=0;//流し評価値が最も小さいもの、を見つからないようにする。
		else if(kuminumber==1)state.kumi_info[1][set]=0;
		else if(kuminumber==2)state.kumi_info[2][set]=0;
		else if(kuminumber==3)state.kumi_info[3][set]=0;
		else if(kuminumber==4)state.kumi_info[4][set]=0;
		else if(kuminumber==5)state.kumi_info[5][set]=0;
		else if(kuminumber==6)state.kumi_info[6][set]=0;
		else if(kuminumber==7)state.kumi_info[7][set]=0;
		else if(kuminumber==8)state.kumi_info[8][set]=0;
		else if(kuminumber==9)state.kumi_info[9][set]=0;
		else if(kuminumber==10)state.kumi_info[10][set]=0;
		
	*/
		/*
		kuminumber=min_kumi_info_kakou(3);//
		
		if(state.kumi_info[kuminumber][0]==1||state.kumi_info[kuminumber][0]==2){
			
			if(state.kumi_info[kuminumber][3]==6&&state.kumi_info[13][0]>=3){//場に何もないときに8のシングルを出そうとしたら
				state.kumi_info[kuminumber][1]=0;
				
			}
		}
		
		kuminumber=min_kumi_info(15);//既に加工されているため
		if(kuminumber==0)copyTable(out_cards,state.kumi0);
		else if(kuminumber==1)copyTable(out_cards,state.kumi1);
		else if(kuminumber==2)copyTable(out_cards,state.kumi2);
		else if(kuminumber==3)copyTable(out_cards,state.kumi3);
		else if(kuminumber==4)copyTable(out_cards,state.kumi4);
		else if(kuminumber==5)copyTable(out_cards,state.kumi5);
		else if(kuminumber==6)copyTable(out_cards,state.kumi6);
		else if(kuminumber==7)copyTable(out_cards,state.kumi7);
		else if(kuminumber==8)copyTable(out_cards,state.kumi8);
		else if(kuminumber==9)copyTable(out_cards,state.kumi9);
		else if(kuminumber==10)copyTable(out_cards,state.kumi10);
		*/

		for (kuminumber = 0; kuminumber <= 10; kuminumber++)
		{

			if (state.kumi_info[kuminumber][1] <= 4 && state.kumi_info[kuminumber][12] >= STRONG && state.kumi_info[kuminumber][0] == 3)
			{

				if (kuminumber == 0)
					copyTable(out_cards, state.kumi0);
				else if (kuminumber == 1)
					copyTable(out_cards, state.kumi1);
				else if (kuminumber == 2)
					copyTable(out_cards, state.kumi2);
				else if (kuminumber == 3)
					copyTable(out_cards, state.kumi3);
				else if (kuminumber == 4)
					copyTable(out_cards, state.kumi4);
				else if (kuminumber == 5)
					copyTable(out_cards, state.kumi5);
				else if (kuminumber == 6)
					copyTable(out_cards, state.kumi6);
				else if (kuminumber == 7)
					copyTable(out_cards, state.kumi7);
				else if (kuminumber == 8)
					copyTable(out_cards, state.kumi8);
				else if (kuminumber == 9)
					copyTable(out_cards, state.kumi9);
				else if (kuminumber == 10)
					copyTable(out_cards, state.kumi10);
			}
		}

		if (beEmptyCards(out_cards) == 1)
		{

			for (kuminumber = 0; kuminumber <= 10; kuminumber++)
			{

				if (state.kumi_info[kuminumber][1] == 3 && state.kumi_info[kuminumber][12] >= STRONG && state.kumi_info[kuminumber][0] == 2)
				{

					if (kuminumber == 0)
						copyTable(out_cards, state.kumi0);
					else if (kuminumber == 1)
						copyTable(out_cards, state.kumi1);
					else if (kuminumber == 2)
						copyTable(out_cards, state.kumi2);
					else if (kuminumber == 3)
						copyTable(out_cards, state.kumi3);
					else if (kuminumber == 4)
						copyTable(out_cards, state.kumi4);
					else if (kuminumber == 5)
						copyTable(out_cards, state.kumi5);
					else if (kuminumber == 6)
						copyTable(out_cards, state.kumi6);
					else if (kuminumber == 7)
						copyTable(out_cards, state.kumi7);
					else if (kuminumber == 8)
						copyTable(out_cards, state.kumi8);
					else if (kuminumber == 9)
						copyTable(out_cards, state.kumi9);
					else if (kuminumber == 10)
						copyTable(out_cards, state.kumi10);
				}
			}
		}

		if (saizyaku_rank < 6)
		{
			//printf("saiyzaku_rank<6\n");
			if (beEmptyCards(out_cards) == 1)
			{

				for (kuminumber = 0; kuminumber <= 10; kuminumber++)
				{

					if (state.kumi_info[kuminumber][1] == 1 && state.kumi_info[kuminumber][12] >= STRONG)
					{

						if (kuminumber == 0)
							copyTable(out_cards, state.kumi0);
						else if (kuminumber == 1)
							copyTable(out_cards, state.kumi1);
						else if (kuminumber == 2)
							copyTable(out_cards, state.kumi2);
						else if (kuminumber == 3)
							copyTable(out_cards, state.kumi3);
						else if (kuminumber == 4)
							copyTable(out_cards, state.kumi4);
						else if (kuminumber == 5)
							copyTable(out_cards, state.kumi5);
						else if (kuminumber == 6)
							copyTable(out_cards, state.kumi6);
						else if (kuminumber == 7)
							copyTable(out_cards, state.kumi7);
						else if (kuminumber == 8)
							copyTable(out_cards, state.kumi8);
						else if (kuminumber == 9)
							copyTable(out_cards, state.kumi9);
						else if (kuminumber == 10)
							copyTable(out_cards, state.kumi10);
					}
				}
			}

			if (beEmptyCards(out_cards) == 1)
			{

				for (kuminumber = 0; kuminumber <= 10; kuminumber++)
				{

					if (state.kumi_info[kuminumber][1] == 2 && state.kumi_info[kuminumber][12] >= STRONG)
					{

						if (kuminumber == 0)
							copyTable(out_cards, state.kumi0);
						else if (kuminumber == 1)
							copyTable(out_cards, state.kumi1);
						else if (kuminumber == 2)
							copyTable(out_cards, state.kumi2);
						else if (kuminumber == 3)
							copyTable(out_cards, state.kumi3);
						else if (kuminumber == 4)
							copyTable(out_cards, state.kumi4);
						else if (kuminumber == 5)
							copyTable(out_cards, state.kumi5);
						else if (kuminumber == 6)
							copyTable(out_cards, state.kumi6);
						else if (kuminumber == 7)
							copyTable(out_cards, state.kumi7);
						else if (kuminumber == 8)
							copyTable(out_cards, state.kumi8);
						else if (kuminumber == 9)
							copyTable(out_cards, state.kumi9);
						else if (kuminumber == 10)
							copyTable(out_cards, state.kumi10);
					}
				}
			}
		}
		else
		{
			//printf("saiyzaku_rank>=6\n");
			if (beEmptyCards(out_cards) == 1)
			{

				for (kuminumber = 0; kuminumber <= 10; kuminumber++)
				{

					if (state.kumi_info[kuminumber][1] == 1 && state.kumi_info[kuminumber][12] >= STRONG && state.kumi_info[kuminumber][3] != 6)
					{

						if (kuminumber == 0)
							copyTable(out_cards, state.kumi0);
						else if (kuminumber == 1)
							copyTable(out_cards, state.kumi1);
						else if (kuminumber == 2)
							copyTable(out_cards, state.kumi2);
						else if (kuminumber == 3)
							copyTable(out_cards, state.kumi3);
						else if (kuminumber == 4)
							copyTable(out_cards, state.kumi4);
						else if (kuminumber == 5)
							copyTable(out_cards, state.kumi5);
						else if (kuminumber == 6)
							copyTable(out_cards, state.kumi6);
						else if (kuminumber == 7)
							copyTable(out_cards, state.kumi7);
						else if (kuminumber == 8)
							copyTable(out_cards, state.kumi8);
						else if (kuminumber == 9)
							copyTable(out_cards, state.kumi9);
						else if (kuminumber == 10)
							copyTable(out_cards, state.kumi10);
					}
				}
			}

			if (beEmptyCards(out_cards) == 1)
			{

				for (kuminumber = 0; kuminumber <= 10; kuminumber++)
				{

					if (state.kumi_info[kuminumber][1] == 1 && state.kumi_info[kuminumber][12] >= STRONG && state.kumi_info[kuminumber][3] == 6)
					{

						if (kuminumber == 0)
							copyTable(out_cards, state.kumi0);
						else if (kuminumber == 1)
							copyTable(out_cards, state.kumi1);
						else if (kuminumber == 2)
							copyTable(out_cards, state.kumi2);
						else if (kuminumber == 3)
							copyTable(out_cards, state.kumi3);
						else if (kuminumber == 4)
							copyTable(out_cards, state.kumi4);
						else if (kuminumber == 5)
							copyTable(out_cards, state.kumi5);
						else if (kuminumber == 6)
							copyTable(out_cards, state.kumi6);
						else if (kuminumber == 7)
							copyTable(out_cards, state.kumi7);
						else if (kuminumber == 8)
							copyTable(out_cards, state.kumi8);
						else if (kuminumber == 9)
							copyTable(out_cards, state.kumi9);
						else if (kuminumber == 10)
							copyTable(out_cards, state.kumi10);
					}
				}
			}

			if (beEmptyCards(out_cards) == 1)
			{

				for (kuminumber = 0; kuminumber <= 10; kuminumber++)
				{

					if (state.kumi_info[kuminumber][1] == 2 && state.kumi_info[kuminumber][12] >= STRONG && state.kumi_info[kuminumber][3] != 6)
					{

						if (kuminumber == 0)
							copyTable(out_cards, state.kumi0);
						else if (kuminumber == 1)
							copyTable(out_cards, state.kumi1);
						else if (kuminumber == 2)
							copyTable(out_cards, state.kumi2);
						else if (kuminumber == 3)
							copyTable(out_cards, state.kumi3);
						else if (kuminumber == 4)
							copyTable(out_cards, state.kumi4);
						else if (kuminumber == 5)
							copyTable(out_cards, state.kumi5);
						else if (kuminumber == 6)
							copyTable(out_cards, state.kumi6);
						else if (kuminumber == 7)
							copyTable(out_cards, state.kumi7);
						else if (kuminumber == 8)
							copyTable(out_cards, state.kumi8);
						else if (kuminumber == 9)
							copyTable(out_cards, state.kumi9);
						else if (kuminumber == 10)
							copyTable(out_cards, state.kumi10);
					}
				}
			}

			if (beEmptyCards(out_cards) == 1)
			{

				for (kuminumber = 0; kuminumber <= 10; kuminumber++)
				{

					if (state.kumi_info[kuminumber][1] == 2 && state.kumi_info[kuminumber][12] >= STRONG && state.kumi_info[kuminumber][3] == 6)
					{

						if (kuminumber == 0)
							copyTable(out_cards, state.kumi0);
						else if (kuminumber == 1)
							copyTable(out_cards, state.kumi1);
						else if (kuminumber == 2)
							copyTable(out_cards, state.kumi2);
						else if (kuminumber == 3)
							copyTable(out_cards, state.kumi3);
						else if (kuminumber == 4)
							copyTable(out_cards, state.kumi4);
						else if (kuminumber == 5)
							copyTable(out_cards, state.kumi5);
						else if (kuminumber == 6)
							copyTable(out_cards, state.kumi6);
						else if (kuminumber == 7)
							copyTable(out_cards, state.kumi7);
						else if (kuminumber == 8)
							copyTable(out_cards, state.kumi8);
						else if (kuminumber == 9)
							copyTable(out_cards, state.kumi9);
						else if (kuminumber == 10)
							copyTable(out_cards, state.kumi10);
					}
				}
			}
		}

		if (beEmptyCards(out_cards) == 1)
		{

			for (kuminumber = 0; kuminumber <= 10; kuminumber++)
			{

				if (state.kumi_info[kuminumber][1] >= 4 && state.kumi_info[kuminumber][12] >= STRONG)
				{

					if (kuminumber == 0)
						copyTable(out_cards, state.kumi0);
					else if (kuminumber == 1)
						copyTable(out_cards, state.kumi1);
					else if (kuminumber == 2)
						copyTable(out_cards, state.kumi2);
					else if (kuminumber == 3)
						copyTable(out_cards, state.kumi3);
					else if (kuminumber == 4)
						copyTable(out_cards, state.kumi4);
					else if (kuminumber == 5)
						copyTable(out_cards, state.kumi5);
					else if (kuminumber == 6)
						copyTable(out_cards, state.kumi6);
					else if (kuminumber == 7)
						copyTable(out_cards, state.kumi7);
					else if (kuminumber == 8)
						copyTable(out_cards, state.kumi8);
					else if (kuminumber == 9)
						copyTable(out_cards, state.kumi9);
					else if (kuminumber == 10)
						copyTable(out_cards, state.kumi10);
				}
			}
		}

		/*
		if(state.kumi_info[13][0]-state.kumi_info[13][7]==1){
			kuminumber=min_kumi_info(12);//1枚あたりの流し評価値が最も小さいものを見つける。
			
			if(kuminumber==0)state.kumi_info[0][set]=0;//流し評価値が最も小さいもの、を見つからないようにする。
			else if(kuminumber==1)state.kumi_info[1][set]=0;
			else if(kuminumber==2)state.kumi_info[2][set]=0;
			else if(kuminumber==3)state.kumi_info[3][set]=0;
			else if(kuminumber==4)state.kumi_info[4][set]=0;
			else if(kuminumber==5)state.kumi_info[5][set]=0;
			else if(kuminumber==6)state.kumi_info[6][set]=0;
			else if(kuminumber==7)state.kumi_info[7][set]=0;
			else if(kuminumber==8)state.kumi_info[8][set]=0;
			else if(kuminumber==9)state.kumi_info[9][set]=0;
			else if(kuminumber==10)state.kumi_info[10][set]=0;
		}
		
		kuminumber=min_kumi_info(4);
		if(kuminumber==0)copyTable(out_cards,state.kumi0);
		else if(kuminumber==1)copyTable(out_cards,state.kumi1);
		else if(kuminumber==2)copyTable(out_cards,state.kumi2);
		else if(kuminumber==3)copyTable(out_cards,state.kumi3);
		else if(kuminumber==4)copyTable(out_cards,state.kumi4);
		else if(kuminumber==5)copyTable(out_cards,state.kumi5);
		else if(kuminumber==6)copyTable(out_cards,state.kumi6);
		else if(kuminumber==7)copyTable(out_cards,state.kumi7);
		else if(kuminumber==8)copyTable(out_cards,state.kumi8);
		else if(kuminumber==9)copyTable(out_cards,state.kumi9);
		else if(kuminumber==10)copyTable(out_cards,state.kumi10);
		*/
		if (beEmptyCards(out_cards) == 1 && state.kumi_info[13][0] == 1)
		{
			copyTable(out_cards, state.kumi0);
		}
	}
}

void my_finish_follow2_rev(int out_cards[8][15], int my_cards[8][15])
{

	clearTable(out_cards);

	if (state.qty == 1)
	{
		my_finish_followSolo2_rev(out_cards, my_cards, state.joker); //一枚のとき
	}
	else
	{
		if (state.sequence == 0)
		{
			my_finish_followGroup2_rev(out_cards, my_cards, state.joker); //枚数組のとき
		}
		else
		{
			my_finish_followSequence2_rev(out_cards, my_cards, state.joker); //階段のとき
		}
	}
}

void my_finish_followSolo2_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};
	int copy_cards[8][15] = {{0}};
	int copy_cards2[8][15] = {{0}};

	int tsuyosa = 0, ba_suit = 0, kumi_suit = 0;

	int kumi_rank = 0;
	int flag = 0;

	int temp3[8][15] = {{0}};
	int temp4[8][15] = {{0}};
	int temp5[8][15] = {{0}};
	int temp6[8][15] = {{0}};

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);
	copyCards(copy_cards, my_cards);

	if (state.suit[0] == 1)
		ba_suit = 0;
	if (state.suit[1] == 1)
		ba_suit = 1;
	if (state.suit[2] == 1)
		ba_suit = 2;
	if (state.suit[3] == 1)
		ba_suit = 3;

	lowCards(temp, copy_cards, state.ord);

	if (state.lock == 1)
	{
		lockCards(temp, state.suit);
	}

	//縛りも考慮して、流せる組をすべて書き出す方法をとることに。
	copyCards(temp3, temp);
	copyCards(temp4, my_cards);
	while (qtyOfCards(temp3) > 0 && flag == 0)
	{
		copyCards(copy_cards, my_cards);

		if (state.find_s3 == 0)
			joker_delete(temp3);

		highSolo2(temp2, temp3, state.joker);

		kumi_rank = get_rank_min(temp2);

		kumi_suit = get_suit(temp2);
		if (kumi_suit == ba_suit)
		{
			tsuyosa = setvalue_single_shibari(temp2, kumi_suit);
		}
		else
		{
			tsuyosa = setvalue_single2(temp2);
		}

		cardsDiff3(temp3, temp2);
		if (tsuyosa >= STRONG)
		{
			cardsDiff3(temp4, temp2); //
		}
	}

	while (qtyOfCards(temp) > 0 && flag == 0)
	{
		copyCards(copy_cards, my_cards);

		if (state.find_s3 == 0)
			joker_delete(temp);

		highSolo2(temp2, temp, state.joker);

		kumi_rank = get_rank_min(temp2);

		kumi_suit = get_suit(temp2);
		if (kumi_suit == ba_suit)
		{
			tsuyosa = setvalue_single_shibari(temp2, kumi_suit);
		}
		else
		{
			tsuyosa = setvalue_single2(temp2);
		}

		cardsDiff3(temp, temp2);

		cardsDiff3(copy_cards, temp2);

		state.finish == 1;
		bunkatsu_pre2(copy_cards);
		state.finish == 0;

		if (tsuyosa >= STRONG && my_lead5_rev_4(temp5, copy_cards) == 1)
		{
			copyTable(out_cards, temp2);
			flag = 2;
			/*
			state.count=state.count+1;
			printf("%d\n",state.game_count);
			printf("kakumeisitekatu_rev   %d   %d\n",state.count,state.game_count);
			outputCards(temp2);
			outputCards(copy_cards);
			*/
		}

		if (tsuyosa >= STRONG && state.kumi_info[13][0] - state.kumi_info[13][7] <= 1)
		{
			copyTable(out_cards, temp2);
			flag = 1;
		}

		if (kumi_suit == ba_suit && flag == 0 && kumi_rank != 6)
		{
			bunkatsu_pre2(temp4);
			if (tsuyosa >= STRONG && state.kumi_info[13][0] - state.kumi_info[13][7] <= 1)
			{
				copyTable(out_cards, temp2);
				flag = 1;
				//printf("yoshi_3\n");
			}
		}

		/*
		if(kumi_suit==ba_suit&&flag==0&&kumi_rank!=6){
			highCards2(copy_cards2,copy_cards,kumi_rank,kumi_suit);
			bunkatsu_pre2(copy_cards2);
			if(tsuyosa>=STRONG&&state.kumi_info[13][0]-state.kumi_info[13][7]<=1){
				copyTable(out_cards,temp2);
				flag=1;
				printf("yoshi2\n");
			}
		}
		*/

		/*
		printf("my_cards\n");
		outputCards(my_cards);
		printf("temp\n");
		outputCards(temp);
		printf("temp2\n");
		outputCards(temp2);
		*/
	}
	if (flag == 2 && beEmptyCards(out_cards) == 1)
	{
		copyTable(out_cards, temp6);
	}
}

void my_finish_followGroup2_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int group[8][15];
	int ngroup[8][15];

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};
	int copy_cards[8][15] = {{0}};

	int tsuyosa = 0, ba_suitsum = 0, kumi_suitsum = 0, j = 0;
	int rank2 = 0, pattern = 0;
	int flag = 0;
	int tsuyosa2 = 0;

	int temp5[8][15] = {{0}};
	int temp6[8][15] = {{0}};
	int temp7[8][15] = {{0}};

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);

	copyTable(copy_cards, my_cards);

	if (state.suit[0] == 1)
		ba_suitsum = ba_suitsum + 1;
	if (state.suit[1] == 1)
		ba_suitsum = ba_suitsum + 2;
	if (state.suit[2] == 1)
		ba_suitsum = ba_suitsum + 4;
	if (state.suit[3] == 1)
		ba_suitsum = ba_suitsum + 8;

	//まず標準プログラムのfollowのように提出手を求める。
	lowCards(temp, copy_cards, state.ord); //場より強いカードを残す
	if (state.lock == 1)
	{								 //ロックされているとき
		lockCards(temp, state.suit); //出せないカードを除去
	}
	makeJGroupTable2(group, temp); //残ったものから枚数組を書き出す
	//makeGroupTable(group,temp);
	nCards2(ngroup, group, state.qty);

	if (qtyOfCards(ngroup) >= state.qty)
	{

		for (rank2 = state.ord - 1; rank2 > 0; rank2--)
		{
			for (pattern = 0; pattern < 6; pattern++)
			{

				copyTable(copy_cards, my_cards);

				Group2(temp2, copy_cards, ngroup, rank2, state.qty, pattern);

				if (qtyOfCards(temp2) == state.qty)
				{

					kumi_suitsum = get_suitsum(temp2);
					if (kumi_suitsum == ba_suitsum)
					{
						tsuyosa = setvalue_pair_shibari(temp2, kumi_suitsum);
						tsuyosa2 = setvalue_pair_shibari3(temp2, kumi_suitsum);
					}
					else
					{
						tsuyosa = setvalue_pair2(temp2);
						tsuyosa2 = setvalue_pair3(temp2);
					}

					cardsDiff3(copy_cards, temp2);

					state.finish == 1;
					bunkatsu_pre2(copy_cards);
					state.finish == 0;

					if (tsuyosa >= STRONG && my_lead5_rev_4(temp5, copy_cards) == 1)
					{
						copyTable(temp6, temp2);
						flag = 2;
						/*
					state.count=state.count+1;
					printf("%d\n",state.game_count);
					printf("kakumeisitekatu_rev   %d   %d\n",state.count,state.game_count);
					outputCards(temp2);
					outputCards(copy_cards);
					*/
					}

					if (tsuyosa >= STRONG && state.kumi_info[13][0] - state.kumi_info[13][7] <= 1)
					{
						copyTable(out_cards, temp2);
						flag = 1;
					}

					if (tsuyosa2 >= (STRONG - 1) * state.qty + 1 && state.kumi_info[13][0] - state.kumi_info[13][7] <= 1 && state.player_number > 3 && state.rest_cards[4][1] == 0 && state.kumi_info[13][6] > 2 && flag == 0)
					{
						copyTable(temp7, temp2);
						flag = 3;
					}
				}
			}
			if (flag == 1)
				break;
		}
	}
	if (flag == 2 && beEmptyCards(out_cards) == 1)
	{
		copyTable(out_cards, temp6);
	}
	if (flag == 3 && beEmptyCards(out_cards) == 1)
	{
		copyTable(out_cards, temp7);
		state.count2 = state.count2 + 1;
		printf("99   %d   %d\n", state.count2, state.game_count);
	}
}

void my_finish_followSequence2_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};

	int copy_cards[8][15] = {{0}};

	int tsuyosa = 0, ba_suit = 0, kumi_suit = 0, j = 0;
	int mark = 0, pattern = 0;
	int tsuyosa2 = 0;

	int temp5[8][15] = {{0}};
	int temp6[8][15] = {{0}};
	int temp7[8][15] = {{0}};

	int flag = 0;

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);

	copyTable(copy_cards, my_cards);

	if (state.suit[0] == 1)
		ba_suit = 0;
	if (state.suit[1] == 1)
		ba_suit = 1;
	if (state.suit[2] == 1)
		ba_suit = 2;
	if (state.suit[3] == 1)
		ba_suit = 3;

	//まず標準プログラムのfollowのように提出手を求める。
	lowCards(temp, my_cards, state.ord); //場より強いカードを残す
	if (state.lock == 1)
	{								 //ロックされているとき
		lockCards(temp, state.suit); //出せないカードを除去
	}

	for (pattern = 11; pattern >= 0; pattern--)
	{
		for (mark = 0; mark < 4; mark++)
		{

			copyCards(copy_cards, my_cards);

			Sequence2(temp2, temp, mark, state.qty, pattern);

			if (qtyOfCards(temp2) == state.qty && get_rank_max(temp2) < state.ord)
			{

				kumi_suit = get_suit(temp2);
				if (kumi_suit == ba_suit)
				{
					tsuyosa = setvalue_kaidan_shibari(temp2, kumi_suit);
					tsuyosa2 = setvalue_kaidan_shibari3(temp2, kumi_suit);
				}
				else
				{
					tsuyosa = setvalue_kaidan2(temp2);
					tsuyosa2 = setvalue_kaidan3(temp2);
				}

				cardsDiff3(copy_cards, temp2);

				state.finish == 1;
				bunkatsu_pre2(copy_cards);
				state.finish == 0;

				if (tsuyosa >= STRONG && my_lead5_rev_4(temp5, copy_cards) == 1)
				{
					copyTable(temp6, temp2);
					flag = 2;
					/*
					state.count=state.count+1;
					printf("%d\n",state.game_count);
					printf("kakumeisitekatu_rev   %d   %d\n",state.count,state.game_count);
					outputCards(temp2);
					outputCards(copy_cards);
					*/
				}

				if (tsuyosa >= STRONG && state.kumi_info[13][0] - state.kumi_info[13][7] <= 1)
				{
					copyTable(out_cards, temp2);
					flag = 1;
				}
				if (tsuyosa2 >= (STRONG - 1) * state.qty + 1 && state.kumi_info[13][0] - state.kumi_info[13][7] <= 1 && state.player_number > 3 && state.rest_cards[4][1] == 0 && state.kumi_info[13][6] > 2 && flag == 0)
				{
					copyTable(temp7, temp2);
					flag = 3;
				}
			}
		}
		if (flag == 1)
			break;
	}
	if (flag == 2 && beEmptyCards(out_cards) == 1)
	{
		copyTable(out_cards, temp6);
	}
	if (flag == 3 && beEmptyCards(out_cards) == 1)
	{
		copyTable(out_cards, temp7);
		state.count2 = state.count2 + 1;
		printf("99   %d   %d\n", state.count2, state.game_count);
	}
}

/*
void my_finish_followSequence2_rev(int out_cards[8][15],int my_cards[8][15],int joker_flag){

  int seq[8][15];
  int nseq[8][15];
 
 	int temp[8][15]={{0}}; 
	int temp2[8][15]={{0}};
	int copy_cards[8][15]={{0}};
	int copy_cards2[8][15]={{0}};
	int copy_cards3[8][15]={{0}};
	int copy_cards4[8][15]={{0}};
	
	int tsuyosa=0,ba_suit=0,kumi_suit=0,j=0;
	
  clearTable(out_cards);
  bunkatsu_pre2(my_cards);

  copyTable(copy_cards,my_cards);
  copyTable(copy_cards2,my_cards);
  copyTable(copy_cards3,my_cards);
  copyTable(copy_cards4,my_cards);
	
  if(state.suit[0]==1)ba_suit=0;
  if(state.suit[1]==1)ba_suit=1;
  if(state.suit[2]==1)ba_suit=2;
  if(state.suit[3]==1)ba_suit=3;
	
 //まず標準プログラムのfollowのように提出手を求める。
  lowCards(temp,my_cards,state.ord);          //場より強いカードを残す
  if(state.lock==1){                           //ロックされているとき
    lockCards(temp,state.suit);                //出せないカードを除去
  }
  makeKaidanTable(seq,temp);                   //階段を書き出す
  nCards2(nseq,seq,state.qty);
 
	
  	 highSequence(temp2,copy_cards,nseq);//弱いほうから1組
	 if(qtyOfCards(temp2)==state.qty){
	 	kumi_suit=get_suit(temp2);
		if(kumi_suit==ba_suit){
			tsuyosa=setvalue_kaidan_shibari(temp2,kumi_suit);
		}else{
			tsuyosa=setvalue_kaidan2(temp2);
		}
	 	
		cardsDiff3(copy_cards,temp2);
	 	bunkatsu_pre2(copy_cards);
		
	 	if(tsuyosa>=STRONG&&state.kumi_info[13][0]-state.kumi_info[13][7]<=1){
	 		copyTable(out_cards,temp2);
	 	}
	 }
	
	if(qtyOfCards(out_cards)==0){
		clearTable(temp2);
		lowSequence(temp2,copy_cards2,nseq);//強いほうから1組
		if(qtyOfCards(temp2)==state.qty){
		 	kumi_suit=get_suit(temp2);
			if(kumi_suit==ba_suit){
				tsuyosa=setvalue_kaidan_shibari(temp2,kumi_suit);
			}else{
				tsuyosa=setvalue_kaidan2(temp2);
			}
		 	
			cardsDiff3(copy_cards2,temp2);
		 	bunkatsu_pre2(copy_cards2);
			
		 	if(tsuyosa>=STRONG&&state.kumi_info[13][0]-state.kumi_info[13][7]<=1){
		 		copyTable(out_cards,temp2);
		 	}
		}
	 }
	
	
	if(state.joker==1){
    //場と同じ枚数の階段が無いときジョーカーを使って探す
   	 	makeJKaidanTable(seq,temp);
   		nCards2(nseq,seq,state.qty);   
		
		if(qtyOfCards(out_cards)==0){	
			clearTable(temp2);
		    highSequence(temp2,copy_cards3,nseq);//弱いほうから1組
			if(qtyOfCards(temp2)==state.qty){
			 	kumi_suit=get_suit(temp2);
				if(kumi_suit==ba_suit){
					tsuyosa=setvalue_kaidan_shibari(temp2,kumi_suit);
				}else{
					tsuyosa=setvalue_kaidan2(temp2);
				}
			 	
				cardsDiff3(copy_cards3,temp2);
			 	bunkatsu_pre2(copy_cards3);
				
			 	if(tsuyosa>=STRONG&&state.kumi_info[13][0]-state.kumi_info[13][7]<=1){
			 		copyTable(out_cards,temp2);
			 	}
		 	}
		}
		
		if(qtyOfCards(out_cards)==0){
			clearTable(temp2);
			lowSequence(temp2,copy_cards4,nseq);//強いほうから1組
			if(qtyOfCards(temp2)==state.qty){
			 	kumi_suit=get_suit(temp2);
				if(kumi_suit==ba_suit){
					tsuyosa=setvalue_kaidan_shibari(temp2,kumi_suit);
				}else{
					tsuyosa=setvalue_kaidan2(temp2);
				}
			 	
				cardsDiff3(copy_cards4,temp2);
			 	bunkatsu_pre2(copy_cards4);
				
			 	if(tsuyosa>=STRONG&&state.kumi_info[13][0]-state.kumi_info[13][7]<=1){
			 		copyTable(out_cards,temp2);
			 	}
			}
		 }
 
 	 }
	
}
*/
void my_finish_follow3_rev(int out_cards[8][15], int my_cards[8][15])
{

	clearTable(out_cards);

	if (state.qty == 1)
	{
		my_finish_followSolo3_rev(out_cards, my_cards, state.joker); //一枚のとき
	}
	else
	{
		if (state.sequence == 0)
		{
			my_finish_followGroup3_rev(out_cards, my_cards, state.joker); //枚数組のとき
		}
		else
		{
			my_finish_followSequence3_rev(out_cards, my_cards, state.joker); //階段のとき
		}
	}
}

void my_finish_followSolo3_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};
	int copy_cards[8][15] = {{0}};

	int tsuyosa = 0, ba_suit = 0, kumi_suit = 0;

	int kumi_rank = 0;
	int flag = 0;

	int temp3[8][15] = {{0}};
	int temp4[8][15] = {{0}};

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);
	copyCards(copy_cards, my_cards);

	if (state.suit[0] == 1)
		ba_suit = 0;
	if (state.suit[1] == 1)
		ba_suit = 1;
	if (state.suit[2] == 1)
		ba_suit = 2;
	if (state.suit[3] == 1)
		ba_suit = 3;

	lowCards(temp, copy_cards, state.ord);

	if (state.lock == 1)
	{
		lockCards(temp, state.suit);
	}

	//縛りも考慮して、流せる組をすべて書き出す方法をとることに。
	copyCards(temp3, temp);
	copyCards(temp4, my_cards);
	while (qtyOfCards(temp3) > 0 && flag == 0)
	{
		copyCards(copy_cards, my_cards);

		if (state.find_s3 == 0)
			joker_delete(temp3);

		highSolo2(temp2, temp3, state.joker);

		kumi_rank = get_rank_min(temp2);

		kumi_suit = get_suit(temp2);
		if (kumi_suit == ba_suit)
		{
			tsuyosa = setvalue_single_shibari(temp2, kumi_suit);
		}
		else
		{
			tsuyosa = setvalue_single2(temp2);
		}

		cardsDiff3(temp3, temp2);
		if (tsuyosa >= STRONG)
		{
			cardsDiff3(temp4, temp2); //
		}
	}

	while (qtyOfCards(temp) > 0 && flag == 0)
	{
		copyCards(copy_cards, my_cards);

		highSolo(temp2, temp, state.joker);

		kumi_suit = get_suit(temp2);
		if (kumi_suit == ba_suit)
		{
			tsuyosa = setvalue_single_shibari(temp2, kumi_suit);
		}
		else
		{
			tsuyosa = setvalue_single2(temp2);
		}

		cardsDiff3(temp, temp2);

		cardsDiff3(copy_cards, temp2);
		bunkatsu_pre2(copy_cards);

		if (state.kumi_info[13][0] - state.kumi_info[13][7] <= 1)
		{
			copyTable(out_cards, temp2);
			flag = 1;
		}

		if (kumi_suit == ba_suit && flag == 0 && kumi_rank != 6)
		{
			bunkatsu_pre2(temp4);
			if (tsuyosa >= STRONG && state.kumi_info[13][0] - state.kumi_info[13][7] <= 1)
			{
				copyTable(out_cards, temp2);
				flag = 1;
				//printf("yoshi_3\n");
			}
		}
		/*
		printf("my_cards\n");
		outputCards(my_cards);
		printf("temp\n");
		outputCards(temp);
		printf("temp2\n");
		outputCards(temp2);
		*/
	}
}

void my_finish_followGroup3_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int group[8][15];
	int ngroup[8][15];

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};
	int copy_cards[8][15] = {{0}};

	int tsuyosa = 0, ba_suitsum = 0, kumi_suitsum = 0, j = 0;
	int rank2 = 0, pattern = 0;
	int flag = 0;

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);

	copyTable(copy_cards, my_cards);

	if (state.suit[0] == 1)
		ba_suitsum = ba_suitsum + 1;
	if (state.suit[1] == 1)
		ba_suitsum = ba_suitsum + 2;
	if (state.suit[2] == 1)
		ba_suitsum = ba_suitsum + 4;
	if (state.suit[3] == 1)
		ba_suitsum = ba_suitsum + 8;

	//まず標準プログラムのfollowのように提出手を求める。
	lowCards(temp, copy_cards, state.ord); //場より強いカードを残す
	if (state.lock == 1)
	{								 //ロックされているとき
		lockCards(temp, state.suit); //出せないカードを除去
	}
	makeJGroupTable2(group, temp);
	//makeGroupTable(group,temp);
	nCards2(ngroup, group, state.qty);

	if (qtyOfCards(ngroup) >= state.qty)
	{

		for (rank2 = state.ord - 1; rank2 > 0; rank2--)
		{
			for (pattern = 0; pattern < 6; pattern++)
			{

				copyTable(copy_cards, my_cards);

				Group2(temp2, copy_cards, ngroup, rank2, state.qty, pattern);

				if (qtyOfCards(temp2) == state.qty)
				{

					kumi_suitsum = get_suitsum(temp2);
					if (kumi_suitsum == ba_suitsum)
					{
						tsuyosa = setvalue_pair_shibari(temp2, kumi_suitsum);
					}
					else
					{
						tsuyosa = setvalue_pair2(temp2);
					}

					cardsDiff3(copy_cards, temp2);
					bunkatsu_pre2(copy_cards);

					if (state.kumi_info[13][0] - state.kumi_info[13][7] <= 1)
					{
						copyTable(out_cards, temp2);
						flag = 1;
					}
				}
			}
			if (flag == 1)
				break;
		}
	}
}

void my_finish_followSequence3_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};

	int copy_cards[8][15] = {{0}};

	int tsuyosa = 0, ba_suit = 0, kumi_suit = 0, j = 0;
	int mark = 0, pattern = 0;

	int flag = 0;

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);

	copyTable(copy_cards, my_cards);

	if (state.suit[0] == 1)
		ba_suit = 0;
	if (state.suit[1] == 1)
		ba_suit = 1;
	if (state.suit[2] == 1)
		ba_suit = 2;
	if (state.suit[3] == 1)
		ba_suit = 3;

	//まず標準プログラムのfollowのように提出手を求める。
	lowCards(temp, my_cards, state.ord); //場より強いカードを残す
	if (state.lock == 1)
	{								 //ロックされているとき
		lockCards(temp, state.suit); //出せないカードを除去
	}

	for (pattern = 11; pattern >= 0; pattern--)
	{
		for (mark = 0; mark < 4; mark++)
		{

			copyCards(copy_cards, my_cards);

			Sequence2(temp2, temp, mark, state.qty, pattern);

			if (qtyOfCards(temp2) == state.qty && get_rank_max(temp2) < state.ord)
			{
				kumi_suit = get_suit(temp2);
				if (kumi_suit == ba_suit)
				{
					tsuyosa = setvalue_kaidan_shibari(temp2, kumi_suit);
				}
				else
				{
					tsuyosa = setvalue_kaidan2(temp2);
				}

				cardsDiff3(copy_cards, temp2);
				bunkatsu_pre2(copy_cards);

				if (state.kumi_info[13][0] - state.kumi_info[13][7] <= 1)
				{
					copyTable(out_cards, temp2);
					flag = 1;
				}
			}
		}
		if (flag == 1)
			break;
	}
}

/*
void my_finish_followSequence3_rev(int out_cards[8][15],int my_cards[8][15],int joker_flag){

  int seq[8][15];
  int nseq[8][15];
 
 	int temp[8][15]={{0}}; 
	int temp2[8][15]={{0}};
	int copy_cards[8][15]={{0}};
	int copy_cards2[8][15]={{0}};
	int copy_cards3[8][15]={{0}};
	int copy_cards4[8][15]={{0}};
	
	int tsuyosa=0,ba_suit=0,kumi_suit=0,j=0;
	
  clearTable(out_cards);
  bunkatsu_pre2(my_cards);

  copyTable(copy_cards,my_cards);
  copyTable(copy_cards2,my_cards);
  copyTable(copy_cards3,my_cards);
  copyTable(copy_cards4,my_cards);
	
  if(state.suit[0]==1)ba_suit=0;
  if(state.suit[1]==1)ba_suit=1;
  if(state.suit[2]==1)ba_suit=2;
  if(state.suit[3]==1)ba_suit=3;
	
 //まず標準プログラムのfollowのように提出手を求める。
  lowCards(temp,my_cards,state.ord);          //場より強いカードを残す
  if(state.lock==1){                           //ロックされているとき
    lockCards(temp,state.suit);                //出せないカードを除去
  }
  makeKaidanTable(seq,temp);                   //階段を書き出す
  nCards2(nseq,seq,state.qty);
 
	
  	 highSequence(temp2,copy_cards,nseq);//弱いほうから1組
	 if(qtyOfCards(temp2)==state.qty){
	 	kumi_suit=get_suit(temp2);
		if(kumi_suit==ba_suit){
			tsuyosa=setvalue_kaidan_shibari(temp2,kumi_suit);
		}else{
			tsuyosa=setvalue_kaidan2(temp2);
		}
	 	
		cardsDiff3(copy_cards,temp2);
	 	bunkatsu_pre2(copy_cards);
		
	 	if(state.kumi_info[13][0]-state.kumi_info[13][7]<=1){
	 		copyTable(out_cards,temp2);
	 	}
	 }
	
	if(qtyOfCards(out_cards)==0){
		clearTable(temp2);
		lowSequence(temp2,copy_cards2,nseq);//強いほうから1組
		if(qtyOfCards(temp2)==state.qty){
		 	kumi_suit=get_suit(temp2);
			if(kumi_suit==ba_suit){
				tsuyosa=setvalue_kaidan_shibari(temp2,kumi_suit);
			}else{
				tsuyosa=setvalue_kaidan2(temp2);
			}
		 	
			cardsDiff3(copy_cards2,temp2);
		 	bunkatsu_pre2(copy_cards2);
			
		 	if(state.kumi_info[13][0]-state.kumi_info[13][7]<=1){
		 		copyTable(out_cards,temp2);
		 	}
		}
	 }
	
	
	if(state.joker==1){
    //場と同じ枚数の階段が無いときジョーカーを使って探す
   	 	makeJKaidanTable(seq,temp);
   		nCards2(nseq,seq,state.qty);   
		
		if(qtyOfCards(out_cards)==0){	
			clearTable(temp2);
		    highSequence(temp2,copy_cards3,nseq);//弱いほうから1組
			if(qtyOfCards(temp2)==state.qty){
			 	kumi_suit=get_suit(temp2);
				if(kumi_suit==ba_suit){
					tsuyosa=setvalue_kaidan_shibari(temp2,kumi_suit);
				}else{
					tsuyosa=setvalue_kaidan2(temp2);
				}
			 	
				cardsDiff3(copy_cards3,temp2);
			 	bunkatsu_pre2(copy_cards3);
				
			 	if(state.kumi_info[13][0]-state.kumi_info[13][7]<=1){
			 		copyTable(out_cards,temp2);
			 	}
		 	}
		}
		
		if(qtyOfCards(out_cards)==0){
			clearTable(temp2);
			lowSequence(temp2,copy_cards4,nseq);//強いほうから1組
			if(qtyOfCards(temp2)==state.qty){
			 	kumi_suit=get_suit(temp2);
				if(kumi_suit==ba_suit){
					tsuyosa=setvalue_kaidan_shibari(temp2,kumi_suit);
				}else{
					tsuyosa=setvalue_kaidan2(temp2);
				}
			 	
				cardsDiff3(copy_cards4,temp2);
			 	bunkatsu_pre2(copy_cards4);
				
			 	if(state.kumi_info[13][0]-state.kumi_info[13][7]<=1){
			 		copyTable(out_cards,temp2);
			 	}
			}
		 }
 
 	 }
	
}
*/
void my_finish_follow4_rev(int out_cards[8][15], int my_cards[8][15])
{

	clearTable(out_cards);

	if (state.qty == 1)
	{
		my_finish_followSolo4_rev(out_cards, my_cards, state.joker); //一枚のとき
	}
	else
	{
		if (state.sequence == 0)
		{
			my_finish_followGroup4_rev(out_cards, my_cards, state.joker); //枚数組のとき
		}
		else
		{
			my_finish_followSequence2_rev(out_cards, my_cards, state.joker); //階段のとき
		}
	}
}

void my_finish_followSolo4_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};
	int copy_cards[8][15] = {{0}};
	int copy_cards2[8][15] = {{0}};
	int copy_cards3[8][15] = {{0}};

	int tsuyosa = 0, ba_suit = 0, kumi_suit = 0;

	int o_cards[8][15] = {{0}};
	int o_cards2[8][15] = {{0}};
	int flag = 0;
	int cards[8][15] = {{0}};
	int cards2[8][15] = {{0}};

	int tsuyosa2 = 0;
	int tsuyosa3 = 0;

	int flag3 = 0;

	int shibari = 0;

	int temp3[8][15] = {{0}};
	int temp4[8][15] = {{0}};

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);
	copyCards(copy_cards, my_cards);

	if (state.suit[0] == 1)
		ba_suit = 0;
	if (state.suit[1] == 1)
		ba_suit = 1;
	if (state.suit[2] == 1)
		ba_suit = 2;
	if (state.suit[3] == 1)
		ba_suit = 3;

	lowCards(temp, copy_cards, state.ord);

	if (state.lock == 1)
	{
		lockCards(temp, state.suit);
	}

	while (qtyOfCards(temp) > 0)
	{ //&&flag3==0
		copyCards(copy_cards, my_cards);

		if (state.find_s3 == 0)
			joker_delete(temp);

		//lowSolo3(temp2,temp,state.joker);//大きい数字から探す。jokerを出すときは、[0][14]に入れる
		highSolo2(temp2, temp, state.joker);

		kumi_suit = get_suit(temp2);
		if (kumi_suit == ba_suit)
		{
			tsuyosa = setvalue_single_shibari(temp2, kumi_suit);
		}
		else
		{
			tsuyosa = setvalue_single2(temp2);
		}

		cardsDiff3(temp, temp2);

		cardsDiff3(copy_cards, temp2);

		copyCards(copy_cards2, copy_cards); //
		copyCards(copy_cards3, copy_cards); //

		//bunkatsu_pre2(copy_cards);

		//my_finish_followSolo4の変更点、ここから
		flag = 0; //リセット
		clearTable(o_cards);
		clearTable(o_cards2);
		clearTable(cards);
		clearTable(cards2);
		tsuyosa2 = 0;
		tsuyosa3 = 0;
		getField2(temp2); //「2」
		shibari = 0;

		if (kumi_suit == ba_suit)
		{
			follow_o_shibari_rev(o_cards, state.rest_cards);
			shibari = 1;
		}
		else
		{
			if (state.player_number == 2)
			{
				follow_o_shibari_rev(o_cards, state.rest_cards);
			}
			follow_o2_rev(o_cards2, state.rest_cards);
		}

		if (qtyOfCards(o_cards) == qtyOfCards(state.rest_cards))
			flag = 99;
		if (qtyOfCards(o_cards2) == qtyOfCards(state.rest_cards))
			flag = 99;
		/*
		printf("temp2\n");
		outputCards(temp2);
		printf("state.ord2          %d\n",state.ord2);
		printf("state.sequence2     %d\n",state.sequence2);
		printf("state.qty2          %d\n",state.qty2);
		outputCards(o_cards);
		outputCards(o_cards2);
		*/

		if (qtyOfCards(o_cards) == 0)
		{
			flag++;
			tsuyosa2 = STRONG;
		}
		else
		{
			getField2(o_cards);
			follow_o_shibari_rev(cards, copy_cards);
			if (qtyOfCards(cards) != 0)
			{
				flag++;
				kumi_suit = get_suit(cards);
				tsuyosa2 = setvalue_single_shibari(cards, kumi_suit);
			}
		}

		if (qtyOfCards(o_cards2) == 0)
		{
			flag++;
			tsuyosa3 = STRONG;
		}
		else
		{
			getField2(o_cards2);
			follow_o2_rev(cards2, copy_cards);
			if (qtyOfCards(cards2) != 0)
			{
				flag++;
				tsuyosa3 = setvalue_single2(cards2);
			}
		}

		if (flag == 2 && tsuyosa2 >= STRONG && tsuyosa3 >= STRONG)
		{
			cardsDiff3(copy_cards2, cards);
			bunkatsu_pre2(copy_cards2);
		}

		if (flag == 2 && state.kumi_info[13][0] - state.kumi_info[13][7] <= 1 && tsuyosa2 >= STRONG && tsuyosa3 >= STRONG)
		{

			cardsDiff3(copy_cards3, cards2);
			bunkatsu_pre2(copy_cards3);

			if (state.kumi_info[13][0] - state.kumi_info[13][7] <= 1)
			{

				copyTable(temp3, temp2);
				//flag3=1;

				if (shibari == 1)
				{
					copyTable(temp4, temp2);
				}

				/*
							printf("my_finish_followsolo4     %d\n",state.game_count);
							printf("temp2\n");
							outputCards(temp2);
							printf("o_cards\n");
							outputCards(o_cards);
							outputCards(cards);
							printf("o_cards2\n");
							outputCards(o_cards2);
							outputCards(cards2);
							
							outputkumi_info(0);
							outputkumi_info(1);
							outputkumi_info(2);
							outputkumi_info(3);
							outputkumi_info(4);
							outputkumi_info(5);
							*/
			}
		}
	}
	/*
	if(qtyOfCards6(state.rest_cards,ba_suit,6)==1&&get_rank_max(temp2)>6){
		
	}else{
		copyTable(out_cards,temp4);
	}
	*/
	copyTable(out_cards, temp4);

	if (beEmptyCards(out_cards) == 1)
	{
		copyTable(out_cards, temp3);
	}
}

void my_finish_followGroup4_rev(int out_cards[8][15], int my_cards[8][15], int joker_flag)
{

	int group[8][15];
	int ngroup[8][15];

	int temp[8][15] = {{0}};
	int temp2[8][15] = {{0}};
	int copy_cards[8][15] = {{0}};
	int copy_cards2[8][15] = {{0}};
	int copy_cards3[8][15] = {{0}};

	int tsuyosa = 0, ba_suitsum = 0, kumi_suitsum = 0, j = 0;

	int rank2 = 0, pattern = 0;
	int flag = 0;

	int o_cards[8][15] = {{0}};
	int o_cards2[8][15] = {{0}};
	int flag2 = 0;
	int cards[8][15] = {{0}};
	int cards2[8][15] = {{0}};

	int tsuyosa2 = 0;
	int tsuyosa3 = 0;
	int shibari = 0;

	int temp3[8][15] = {{0}};
	int temp4[8][15] = {{0}};

	clearTable(out_cards);
	bunkatsu_pre2(my_cards);

	copyTable(copy_cards, my_cards);

	if (state.suit[0] == 1)
		ba_suitsum = ba_suitsum + 1;
	if (state.suit[1] == 1)
		ba_suitsum = ba_suitsum + 2;
	if (state.suit[2] == 1)
		ba_suitsum = ba_suitsum + 4;
	if (state.suit[3] == 1)
		ba_suitsum = ba_suitsum + 8;

	//まず標準プログラムのfollowのように提出手を求める。
	lowCards(temp, copy_cards, state.ord); //場より強いカードを残す
	if (state.lock == 1)
	{								 //ロックされているとき
		lockCards(temp, state.suit); //出せないカードを除去
	}
	makeJGroupTable2(group, temp); //残ったものから枚数組を書き出す
								   //makeGroupTable(group,temp);
	nCards2(ngroup, group, state.qty);

	//outputCards(ngroup);

	if (qtyOfCards(ngroup) >= state.qty)
	{

		//for(rank2=state.ord+1;rank2<14;rank2++){
		for (rank2 = 1; rank2 < state.ord; rank2++)
		{ //大きい数字から検討
			for (pattern = 0; pattern < 6; pattern++)
			{

				copyTable(copy_cards, my_cards);

				Group2(temp2, copy_cards, ngroup, rank2, state.qty, pattern);

				if (qtyOfCards(temp2) == state.qty)
				{

					//outputCards(temp2);

					kumi_suitsum = get_suitsum(temp2);
					if (kumi_suitsum == ba_suitsum)
					{
						tsuyosa = setvalue_pair_shibari(temp2, kumi_suitsum);
					}
					else
					{
						tsuyosa = setvalue_pair2(temp2);
					}

					cardsDiff3(copy_cards, temp2);

					copyCards(copy_cards2, copy_cards); //
					copyCards(copy_cards3, copy_cards); //

					//bunkatsu_pre2(copy_cards);

					//my_finish_followGroup4の変更点、ここから
					flag2 = 0; //リセット
					clearTable(o_cards);
					clearTable(o_cards2);
					clearTable(cards);
					clearTable(cards2);
					tsuyosa2 = 0;
					tsuyosa3 = 0;
					shibari = 0;

					getField2(temp2); //「2」

					if (kumi_suitsum == ba_suitsum)
					{
						follow_o_shibari_rev(o_cards, state.rest_cards);
						shibari = 1;
					}
					else
					{
						if (state.player_number == 2)
						{
							follow_o_shibari_rev(o_cards, state.rest_cards);
						}
						follow_o2_rev(o_cards2, state.rest_cards);
					}

					if (state.player_number == 2)
					{
						if (qtyOfCards(o_cards) == qtyOfCards(state.rest_cards))
							flag = 99;
						if (qtyOfCards(o_cards2) == qtyOfCards(state.rest_cards))
							flag = 99;
					}
					/*
						printf("temp2\n");
						outputCards(temp2);
						printf("state.ord2          %d\n",state.ord2);
						printf("state.sequence2     %d\n",state.sequence2);
						printf("state.qty2          %d\n",state.qty2);
						outputCards(o_cards);
						outputCards(o_cards2);
						*/

					if (qtyOfCards(o_cards) == 0)
					{
						flag2++;
						tsuyosa2 = STRONG;
					}
					else
					{
						getField2(o_cards);
						follow_o_shibari_rev(cards, copy_cards);
						if (qtyOfCards(cards) != 0)
						{
							flag2++;
							kumi_suitsum = get_suitsum(cards);
							tsuyosa2 = setvalue_pair_shibari(cards, kumi_suitsum);
						}
					}

					if (qtyOfCards(o_cards2) == 0)
					{
						flag2++;
						tsuyosa3 = STRONG;
					}
					else
					{
						getField2(o_cards2);
						follow_o2_rev(cards2, copy_cards);
						if (qtyOfCards(cards2) != 0)
						{
							flag2++;
							tsuyosa3 = setvalue_pair2(cards2);
						}
					}

					if (flag2 == 2 && tsuyosa2 >= STRONG && tsuyosa3 >= STRONG)
					{
						cardsDiff3(copy_cards2, cards);
						bunkatsu_pre2(copy_cards2);
					}

					if (flag2 == 2 && state.kumi_info[13][0] - state.kumi_info[13][7] <= 1 && tsuyosa2 >= STRONG && tsuyosa3 >= STRONG)
					{

						cardsDiff3(copy_cards3, cards2);
						bunkatsu_pre2(copy_cards3);

						if (state.kumi_info[13][0] - state.kumi_info[13][7] <= 1)
						{

							copyTable(temp3, temp2);
							//flag3=1;

							if (shibari == 1)
							{
								copyTable(temp4, temp2);
							}
							/*
											printf("my_finish_followGroup4     %d\n",state.game_count);
											printf("temp2\n");
											outputCards(temp2);
											printf("o_cards\n");
											outputCards(o_cards);
											outputCards(cards);
											printf("o_cards2\n");
											outputCards(o_cards2);
											outputCards(cards2);
								
											printf("temp2\n");
											outputCards(temp2);
											printf("tsuyosa2     %d\n",tsuyosa2);
											printf("tsuyosa3     %d\n",tsuyosa3);
								
											outputkumi_info(0);
											outputkumi_info(1);
											outputkumi_info(2);
											outputkumi_info(3);
											outputkumi_info(4);
											outputkumi_info(5);
								*/
						}
					}
				}
			}
		}
	}
	copyTable(out_cards, temp4);

	if (beEmptyCards(out_cards) == 1)
	{
		copyTable(out_cards, temp3);
	}
}

int setvalue_single(int cards[8][15], int kaisuu)
{

	int i = 0, j = 0, a = 0, b = 0, value, teisuu = 100, count = 0, eight = 0;
	int value_rev = 0, count_rev = 0, joker = 0;

	//clearkumi_info(kaisuu);

	for (i = 0; i < 4; i++)
	{
		for (j = 1; j < 14; j++)
		{
			if (cards[i][j] == 1)
			{
				a = i;
				b = j;
				if (j == 6)
					eight = eight + 1;
			}
			if (cards[i][j] == 2)
				joker = 1;
		}
	}
	for (i = 0; i < 4; i++)
	{
		for (j = b + 1; j < 14; j++)
		{
			if (state.submitted_cards_plus_hands[i][j] == 0)
			{
				count++;
			}
		}
	}

	//革命用の評価値の設定
	for (i = 0; i < 4; i++)
	{
		for (j = b - 1; j > 0; j--)
		{
			if (state.submitted_cards_plus_hands[i][j] == 0)
			{
				count_rev++;
			}
		}
	}

	if (state.have_s3 == 1)
	{
		value = teisuu - count;
		value_rev = teisuu - count_rev;
	}
	else
	{
		if (state.player_number > 2)
		{

			if (state.submit_s3 == 0)
			{
				if (state.have_s3 == 1)
				{
					value = teisuu - count;
					value_rev = teisuu - count_rev;
				}
				else
				{
					value = teisuu - count - state.joker_risk;
					value_rev = teisuu - count_rev - state.joker_risk;
				}
			}
			else
			{
				value = teisuu - count - state.joker_risk;
				value_rev = teisuu - count_rev - state.joker_risk;
			}
		}
		else
		{
			value = teisuu - count - state.joker_risk;
			value_rev = teisuu - count_rev - state.joker_risk;
		}
	}

	/*
	if(state.submit_s3==0){
		value=teisuu - count;
		value_rev=teisuu-count_rev;
	}else{
		value=teisuu - count-state.joker_risk;
		value_rev=teisuu-count_rev-state.joker_risk;
	}
	*/

	state.kumi_info[kaisuu][0] = 1;
	state.kumi_info[kaisuu][1] = 1;
	state.kumi_info[kaisuu][2] = a;
	state.kumi_info[kaisuu][3] = b;
	state.kumi_info[kaisuu][4] = b;
	state.kumi_info[kaisuu][5] = value; //強さ
	state.kumi_info[kaisuu][6] = value;
	state.kumi_info[kaisuu][7] = value_rev; //強さ
	state.kumi_info[kaisuu][8] = value_rev;

	if (eight != 0)
	{
		state.kumi_info[kaisuu][9] = 100;
		state.kumi_info[kaisuu][10] = 100;
		state.kumi_info[kaisuu][11] = 100;
		state.kumi_info[kaisuu][12] = 100;
	}
	else
	{
		state.kumi_info[kaisuu][9] = value; //流し
		state.kumi_info[kaisuu][10] = value;
		state.kumi_info[kaisuu][11] = value_rev;
		state.kumi_info[kaisuu][12] = value_rev;
	}

	state.kumi_info[kaisuu][13] = 0;
	state.kumi_info[kaisuu][14] = 0;

	if (cards[4][1] != 0 || cards[0][0] == 2 || cards[0][14] == 2 || joker == 1)
	{
		value = 100 - state.s3_risk;

		if (state.rev == 0)
		{
			clearCards(cards);
			cards[0][14] = 2;
		}
		else
		{
			clearCards(cards);
			cards[0][0] = 2;
		}

		state.kumi_info[kaisuu][0] = 1;
		state.kumi_info[kaisuu][1] = 1;
		state.kumi_info[kaisuu][2] = 4;	 //仮
		state.kumi_info[kaisuu][3] = 14; //仮
		state.kumi_info[kaisuu][4] = 0;
		state.kumi_info[kaisuu][5] = value;
		state.kumi_info[kaisuu][6] = value;
		state.kumi_info[kaisuu][7] = value;
		state.kumi_info[kaisuu][8] = value;
		state.kumi_info[kaisuu][9] = value;
		state.kumi_info[kaisuu][10] = value;
		state.kumi_info[kaisuu][11] = value;
		state.kumi_info[kaisuu][12] = value;
		state.kumi_info[kaisuu][13] = 1;
		state.kumi_info[kaisuu][14] = 0;
	}

	//outputkumi_info(kaisuu);
	if (state.rev == 0)
	{
		return value;
	}
	else
	{
		return value_rev;
	}
}

int setvalue_pair(int cards[8][15], int kaisuu)
{

	int i = 0, j = 0, a = 0, b = 0, value, teisuu = 100, count = 0, maisuu = 0, suitsum = 0, eight = 0;
	int value_rev = 0, count_rev = 0;

	//suitを1,2,4,8の4つにわけて判断することにする。

	//clearkumi_info(kaisuu);

	maisuu = qtyOfCards(cards);

	for (i = 0; i < 5; i++)
	{
		for (j = 1; j < 14; j++)
		{
			if (cards[i][j] == 1 || cards[i][j] == 2)
			{
				a = i;
				b = j;

				if (i == 0)
					suitsum = suitsum + 1; //マークを1,2,4,8の組み合わせで表すことにした。
				if (i == 1)
					suitsum = suitsum + 2;
				if (i == 2)
					suitsum = suitsum + 4;
				if (i == 3)
					suitsum = suitsum + 8;

				if (j == 6)
					eight = eight + 1;
				if (cards[i][j] == 2)
					state.kumi_info[kaisuu][13] = 1;
			}
		}
	}

	if (maisuu == 2)
	{
		for (j = b + 1; j < 14; j++)
		{
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0)
				count++;
			if (state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count++;
			if (state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count++;
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
			if (state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
		}
	}

	if (maisuu == 3)
	{
		for (j = b + 1; j < 14; j++)
		{
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count++;
			if (state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
		}
	}

	if (maisuu == 4)
	{
		for (j = b - 1; j > 0; j--)
		{ //革命
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
		}
	}

	//革命用の評価値の設定

	if (maisuu == 2)
	{
		for (j = b - 1; j > 0; j--)
		{
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0)
				count_rev++;
			if (state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count_rev++;
			if (state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count_rev++;
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
			if (state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
		}
	}

	if (maisuu == 3)
	{
		for (j = b - 1; j > 0; j--)
		{
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count_rev++;
			if (state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
		}
	}

	if (maisuu == 4)
	{
		for (j = b + 1; j < 14; j++)
		{ //革命
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
		}
	}
	//ここまで

	//value=teisuu * maisuu - count -state.joker_risk +state.player_number_point*maisuu;
	//value_rev=teisuu * maisuu - count_rev -state.joker_risk +state.player_number_point*maisuu;

	value = teisuu * maisuu - count;
	value_rev = teisuu * maisuu - count_rev;
	/*
	if(state.player_number<=4){
		value=teisuu * maisuu -count;
		value_rev=teisuu * maisuu -count_rev;
	}else{
		value=teisuu * maisuu -count+maisuu-1;
		value_rev=teisuu * maisuu -count_rev+maisuu-1;
	}
	*/

	state.kumi_info[kaisuu][0] = 2;
	state.kumi_info[kaisuu][1] = maisuu;
	state.kumi_info[kaisuu][2] = suitsum;
	state.kumi_info[kaisuu][3] = b;
	state.kumi_info[kaisuu][4] = b;
	state.kumi_info[kaisuu][5] = value;
	state.kumi_info[kaisuu][6] = value / maisuu;
	state.kumi_info[kaisuu][7] = value_rev;
	state.kumi_info[kaisuu][8] = value_rev / maisuu;

	if (eight != 0)
	{
		state.kumi_info[kaisuu][9] = 100 * maisuu;
		state.kumi_info[kaisuu][10] = 100;
		state.kumi_info[kaisuu][11] = 100 * maisuu;
		state.kumi_info[kaisuu][12] = 100;
	}
	else
	{
		state.kumi_info[kaisuu][9] = value; //流し
		state.kumi_info[kaisuu][10] = value / maisuu;
		state.kumi_info[kaisuu][11] = value_rev; //流し
		state.kumi_info[kaisuu][12] = value_rev / maisuu;
	}

	//state.kumi_info[kaisuu][13]=0;//仮
	if (maisuu >= 4)
	{
		state.kumi_info[kaisuu][14] = 1;
	}

	//outputkumi_info(kaisuu);

	if (state.rev == 0)
	{
		return value;
	}
	else
	{
		return value_rev;
	}
}

int setvalue_kaidan(int cards[8][15], int kaisuu)
{
	int i = 0, j = 0, a = 0, b = 0, value, teisuu = 100, count = 0, maisuu = 0, eight = 0;
	int c = 0, d = 0, value_rev = 0, count_rev = 0;

	//clearkumi_info(kaisuu);

	maisuu = qtyOfCards(cards);

	for (i = 0; i < 5; i++)
	{
		for (j = 1; j < 14; j++)
		{
			if (cards[i][j] == 1 || cards[i][j] == 2)
			{
				a = i;
				b = j;

				if (j == 6)
					eight = eight + 1;
				if (cards[i][j] == 2)
					state.kumi_info[kaisuu][13] = 1;
			}
		}
	}

	if (maisuu == 3)
	{
		for (i = 0; i < 4; i++)
		{
			for (j = b + 1; j < 15 - maisuu; j++)
			{
				if (state.submitted_cards_plus_hands[i][j] == 0 && state.submitted_cards_plus_hands[i][j + 1] == 0 && state.submitted_cards_plus_hands[i][j + 2] == 0)
					count++;
			}
		}
	}

	if (maisuu == 4)
	{
		for (i = 0; i < 4; i++)
		{
			for (j = b + 1; j < 15 - maisuu; j++)
			{
				if (state.submitted_cards_plus_hands[i][j] == 0 && state.submitted_cards_plus_hands[i][j + 1] == 0 && state.submitted_cards_plus_hands[i][j + 2] == 0 && state.submitted_cards_plus_hands[i][j + 3] == 0)
					count++;
			}
		}
	}

	if (maisuu == 5)
	{
		for (i = 0; i < 4; i++)
		{
			for (j = b + 1; j < 15 - maisuu; j++)
			{
				if (state.submitted_cards_plus_hands[i][j] == 0 && state.submitted_cards_plus_hands[i][j + 1] == 0 && state.submitted_cards_plus_hands[i][j + 2] == 0 && state.submitted_cards_plus_hands[i][j + 3] == 0 && state.submitted_cards_plus_hands[i][j + 4] == 0)
					count++;
			}
		}
	}

	//革命用
	for (i = 0; i < 5; i++)
	{
		for (j = 13; j > 0; j--)
		{
			if (cards[i][j] == 1 || cards[i][j] == 2)
			{
				c = i;
				d = j;
			}
		}
	}

	if (maisuu == 3)
	{
		for (i = 0; i < 4; i++)
		{
			for (j = d - 1; j > maisuu - 1; j--)
			{
				if (state.submitted_cards_plus_hands[i][j] == 0 && state.submitted_cards_plus_hands[i][j - 1] == 0 && state.submitted_cards_plus_hands[i][j - 2] == 0)
					count_rev++;
			}
		}
	}

	if (maisuu == 4)
	{
		for (i = 0; i < 4; i++)
		{
			for (j = d - 1; j > maisuu - 1; j--)
			{
				if (state.submitted_cards_plus_hands[i][j] == 0 && state.submitted_cards_plus_hands[i][j - 1] == 0 && state.submitted_cards_plus_hands[i][j - 2] == 0 && state.submitted_cards_plus_hands[i][j - 3] == 0)
					count_rev++;
			}
		}
	}

	if (maisuu == 5)
	{
		for (i = 0; i < 4; i++)
		{
			for (j = d - 1; j > maisuu - 1; j--)
			{
				if (state.submitted_cards_plus_hands[i][j] == 0 && state.submitted_cards_plus_hands[i][j - 1] == 0 && state.submitted_cards_plus_hands[i][j - 2] == 0 && state.submitted_cards_plus_hands[i][j - 3] == 0 && state.submitted_cards_plus_hands[i][j - 4] == 0)
					count_rev++;
			}
		}
	}
	//ここまで

	//value=teisuu * maisuu - count -state.joker_risk+state.player_number_point*maisuu;
	//value_rev=teisuu * maisuu - count_rev -state.joker_risk+state.player_number_point*maisuu;

	value = teisuu * maisuu - count;
	value_rev = teisuu * maisuu - count_rev;

	state.kumi_info[kaisuu][0] = 3;
	state.kumi_info[kaisuu][1] = maisuu;
	state.kumi_info[kaisuu][2] = a;
	state.kumi_info[kaisuu][3] = b - maisuu + 1;
	state.kumi_info[kaisuu][4] = b;
	state.kumi_info[kaisuu][5] = value;
	state.kumi_info[kaisuu][6] = value / maisuu;
	state.kumi_info[kaisuu][7] = value_rev;
	state.kumi_info[kaisuu][8] = value_rev / maisuu;

	if (eight != 0)
	{
		state.kumi_info[kaisuu][9] = 100 * maisuu;
		state.kumi_info[kaisuu][10] = 100;
		state.kumi_info[kaisuu][11] = 100 * maisuu;
		state.kumi_info[kaisuu][12] = 100;
	}
	else
	{
		state.kumi_info[kaisuu][9] = value; //流し
		state.kumi_info[kaisuu][10] = value / maisuu;
		state.kumi_info[kaisuu][11] = value_rev; //流し
		state.kumi_info[kaisuu][12] = value_rev / maisuu;
	}

	//state.kumi_info[kaisuu][13]=0;//仮
	if (maisuu >= 5)
	{
		state.kumi_info[kaisuu][14] = 1;
	}

	//outputkumi_info(kaisuu);

	if (state.rev == 0)
	{
		return value;
	}
	else
	{
		return value_rev;
	}
}

int setvalue_hands()
{
	//手札の情報を設定（state.kumi_info[13][]）
	int c = 0, kuminumber = 0;

	for (c = 0; c < 13; c++)
	{ //手札評価値等の設定
		if (state.kumi_info[c][1] >= 1)
			state.kumi_info[13][0] = state.kumi_info[13][0] + 1;
		if (state.kumi_info[c][14] == 1)
			state.kumi_info[13][1] = state.kumi_info[13][1] + 1;
		if (state.kumi_info[c][5] >= 1)
			state.kumi_info[13][2] = state.kumi_info[13][2] + state.kumi_info[c][5];
		if (state.kumi_info[c][7] >= 1)
			state.kumi_info[13][3] = state.kumi_info[13][3] + state.kumi_info[c][7];
		if (state.kumi_info[c][9] >= 1)
			state.kumi_info[13][4] = state.kumi_info[13][4] + state.kumi_info[c][9];
		if (state.kumi_info[c][11] >= 1)
			state.kumi_info[13][5] = state.kumi_info[13][5] + state.kumi_info[c][11];

		if (state.kumi_info[c][10] >= STRONG)
			state.kumi_info[13][6] = state.kumi_info[13][6] + 1; //一枚当たりの流し評価値が規定値を超えている場合、カウントする。
		//||(state.kumi_info[c][1]>=3&&state.player_number==5&&qtyOfCards(state.rest_cards)>=39)
		if (state.kumi_info[c][12] >= STRONG)
			state.kumi_info[13][7] = state.kumi_info[13][7] + 1; //一枚当たりの流し評価値が規定値を超えている場合、カウントする。

		if (state.kumi_info[c][1] >= 1)
			state.kumi_info[13][8] = state.kumi_info[13][8] + state.kumi_info[c][1]; //手札の枚数

		if (state.kumi_info[c][1] == 1)
			state.kumi_info[13][9] = state.kumi_info[13][9] + 1; //シングルの組数
		if (state.kumi_info[c][1] == 2)
			state.kumi_info[13][10] = state.kumi_info[13][10] + 1; //ペア（2枚組）の組数
		if (state.kumi_info[c][1] == 3 && state.kumi_info[c][0] == 2)
			state.kumi_info[13][11] = state.kumi_info[13][11] + 1; //ペア（3枚組）の組数
		if (state.kumi_info[c][1] == 3 && state.kumi_info[c][0] == 3)
			state.kumi_info[13][12] = state.kumi_info[13][12] + 1; //階段（3枚組）の組数
		if (state.kumi_info[c][1] >= 4 && state.kumi_info[c][0] == 2)
			state.kumi_info[13][13] = state.kumi_info[13][13] + 1; //ペア（4枚組以上）の組数
		if (state.kumi_info[c][1] >= 4 && state.kumi_info[c][0] == 3)
			state.kumi_info[13][14] = state.kumi_info[13][14] + 1; //階段（4枚組以上）の組数

		if (state.kumi_info[c][10] >= 1)
			state.kumi_info[13][15] = state.kumi_info[13][15] + state.kumi_info[c][10];
		if (state.kumi_info[c][12] >= 1)
			state.kumi_info[13][16] = state.kumi_info[13][16] + state.kumi_info[c][12];

		if (state.kumi_info[c][1] == 1 && state.kumi_info[c][4] >= state.kumi_info[13][21])
			state.kumi_info[13][21] = state.kumi_info[c][4];
		if (state.kumi_info[c][1] == 2 && state.kumi_info[c][4] >= state.kumi_info[13][22])
			state.kumi_info[13][22] = state.kumi_info[c][4];
		if (state.kumi_info[c][1] == 3 && state.kumi_info[c][0] == 2 && state.kumi_info[c][4] >= state.kumi_info[13][23])
			state.kumi_info[13][23] = state.kumi_info[c][4];

		if (state.rev == 0)
		{

			if (state.kumi_info[c][1] == 1 && state.kumi_info[c][10] >= STRONG && state.kumi_info[c][3] != 6 && state.kumi_info[c][3] != 13)
				state.kumi_info[13][24] = state.kumi_info[13][24] + 1;
			if (state.kumi_info[c][1] == 1 && state.kumi_info[c][10] >= STRONG && state.kumi_info[c][3] == 6)
				state.kumi_info[13][25] = state.kumi_info[13][25] + 1;
			if (state.kumi_info[c][1] == 2 && state.kumi_info[c][10] >= STRONG && state.kumi_info[c][3] != 6 && state.kumi_info[c][3] != 13)
				state.kumi_info[13][26] = state.kumi_info[13][26] + 1;
			if (state.kumi_info[c][1] == 2 && state.kumi_info[c][10] >= STRONG && state.kumi_info[c][3] == 6)
				state.kumi_info[13][27] = state.kumi_info[13][27] + 1;
			if (state.kumi_info[c][1] == 3 && state.kumi_info[c][10] >= STRONG && state.kumi_info[c][3] != 6 && state.kumi_info[c][3] != 13 && state.kumi_info[c][0] == 2)
				state.kumi_info[13][28] = state.kumi_info[13][28] + 1;
			if (state.kumi_info[c][1] == 3 && state.kumi_info[c][10] >= STRONG && state.kumi_info[c][3] == 6 && state.kumi_info[c][0] == 2)
				state.kumi_info[13][29] = state.kumi_info[13][29] + 1;
			if (state.kumi_info[c][1] == 3 && state.kumi_info[c][10] >= STRONG && state.kumi_info[c][0] == 3)
				state.kumi_info[13][30] = state.kumi_info[13][30] + 1;
			if (state.kumi_info[c][1] >= 4 && state.kumi_info[c][10] >= STRONG && state.kumi_info[c][3] != 6 && state.kumi_info[c][3] != 13 && state.kumi_info[c][0] == 2)
				state.kumi_info[13][31] = state.kumi_info[13][31] + 1;
			if (state.kumi_info[c][1] >= 4 && state.kumi_info[c][10] >= STRONG && state.kumi_info[c][3] == 6 && state.kumi_info[c][0] == 2)
				state.kumi_info[13][32] = state.kumi_info[13][32] + 1;
			if (state.kumi_info[c][1] >= 4 && state.kumi_info[c][10] >= STRONG && state.kumi_info[c][0] == 3)
				state.kumi_info[13][33] = state.kumi_info[13][33] + 1;

			if (state.kumi_info[c][3] == 13 && (state.kumi_info[c][0] == 1 || state.kumi_info[c][0] == 2))
				state.kumi_info[13][34] = state.kumi_info[13][34] + 1;

			if (state.kumi_info[c][10] >= STRONG - 1)
				state.kumi_info[13][35] = state.kumi_info[13][35] + 1;

			if (state.kumi_info[c][1] == 1 && state.kumi_info[c][10] >= STRONG - 1 && state.kumi_info[c][3] != 6 && state.kumi_info[c][3] != 13)
				state.kumi_info[13][36] = state.kumi_info[13][36] + 1;
			if (state.kumi_info[c][1] == 1 && state.kumi_info[c][10] >= STRONG - 1 && state.kumi_info[c][3] == 6)
				state.kumi_info[13][37] = state.kumi_info[13][37] + 1;
			if (state.kumi_info[c][1] == 2 && state.kumi_info[c][10] >= STRONG - 1 && state.kumi_info[c][3] != 6 && state.kumi_info[c][3] != 13)
				state.kumi_info[13][38] = state.kumi_info[13][38] + 1;
			if (state.kumi_info[c][1] == 2 && state.kumi_info[c][10] >= STRONG - 1 && state.kumi_info[c][3] == 6)
				state.kumi_info[13][39] = state.kumi_info[13][39] + 1;
			if (state.kumi_info[c][1] == 3 && state.kumi_info[c][10] >= STRONG - 1 && state.kumi_info[c][3] != 6 && state.kumi_info[c][3] != 13 && state.kumi_info[c][0] == 2)
				state.kumi_info[13][40] = state.kumi_info[13][40] + 1;
			if (state.kumi_info[c][1] == 3 && state.kumi_info[c][10] >= STRONG - 1 && state.kumi_info[c][3] == 6 && state.kumi_info[c][0] == 2)
				state.kumi_info[13][41] = state.kumi_info[13][41] + 1;
			if (state.kumi_info[c][1] == 3 && state.kumi_info[c][10] >= STRONG - 1 && state.kumi_info[c][0] == 3)
				state.kumi_info[13][42] = state.kumi_info[13][42] + 1;
			if (state.kumi_info[c][1] >= 4 && state.kumi_info[c][10] >= STRONG - 1 && state.kumi_info[c][3] != 6 && state.kumi_info[c][3] != 13 && state.kumi_info[c][0] == 2)
				state.kumi_info[13][43] = state.kumi_info[13][43] + 1;
			if (state.kumi_info[c][1] >= 4 && state.kumi_info[c][10] >= STRONG - 1 && state.kumi_info[c][3] == 6 && state.kumi_info[c][0] == 2)
				state.kumi_info[13][44] = state.kumi_info[13][44] + 1;
			if (state.kumi_info[c][1] >= 4 && state.kumi_info[c][10] >= STRONG - 1 && state.kumi_info[c][0] == 3)
				state.kumi_info[13][45] = state.kumi_info[13][45] + 1;

			if (state.kumi_info[c][1] == 1 && state.kumi_info[c][3] == 13)
				state.kumi_info[13][46] = state.kumi_info[13][46] + 1;
			if (state.kumi_info[c][1] == 2 && state.kumi_info[c][3] == 13)
				state.kumi_info[13][47] = state.kumi_info[13][47] + 1;
			if (state.kumi_info[c][1] == 3 && state.kumi_info[c][3] == 13 && state.kumi_info[c][0] == 2)
				state.kumi_info[13][48] = state.kumi_info[13][48] + 1;

			if (state.kumi_info[c][10] >= STRONG - 2)
				state.kumi_info[13][49] = state.kumi_info[13][49] + 1;
			if (state.kumi_info[c][10] >= STRONG - 4)
				state.kumi_info[13][50] = state.kumi_info[13][50] + 1;
		}
		else
		{

			if (state.kumi_info[c][1] == 1 && state.kumi_info[c][12] >= STRONG && state.kumi_info[c][3] != 6 && state.kumi_info[c][3] != 1)
				state.kumi_info[13][24] = state.kumi_info[13][24] + 1;
			if (state.kumi_info[c][1] == 1 && state.kumi_info[c][12] >= STRONG && state.kumi_info[c][3] == 6)
				state.kumi_info[13][25] = state.kumi_info[13][25] + 1;
			if (state.kumi_info[c][1] == 2 && state.kumi_info[c][12] >= STRONG && state.kumi_info[c][3] != 6 && state.kumi_info[c][3] != 1)
				state.kumi_info[13][26] = state.kumi_info[13][26] + 1;
			if (state.kumi_info[c][1] == 2 && state.kumi_info[c][12] >= STRONG && state.kumi_info[c][3] == 6)
				state.kumi_info[13][27] = state.kumi_info[13][27] + 1;
			if (state.kumi_info[c][1] == 3 && state.kumi_info[c][12] >= STRONG && state.kumi_info[c][3] != 6 && state.kumi_info[c][3] != 1 && state.kumi_info[c][0] == 2)
				state.kumi_info[13][28] = state.kumi_info[13][28] + 1;
			if (state.kumi_info[c][1] == 3 && state.kumi_info[c][12] >= STRONG && state.kumi_info[c][3] == 6 && state.kumi_info[c][0] == 2)
				state.kumi_info[13][29] = state.kumi_info[13][29] + 1;
			if (state.kumi_info[c][1] == 3 && state.kumi_info[c][12] >= STRONG && state.kumi_info[c][0] == 3)
				state.kumi_info[13][30] = state.kumi_info[13][30] + 1;
			if (state.kumi_info[c][1] == 4 && state.kumi_info[c][12] >= STRONG && state.kumi_info[c][3] != 6 && state.kumi_info[c][3] != 1 && state.kumi_info[c][0] == 2)
				state.kumi_info[13][31] = state.kumi_info[13][31] + 1;
			if (state.kumi_info[c][1] == 4 && state.kumi_info[c][12] >= STRONG && state.kumi_info[c][3] == 6 && state.kumi_info[c][0] == 2)
				state.kumi_info[13][32] = state.kumi_info[13][32] + 1;
			if (state.kumi_info[c][1] == 4 && state.kumi_info[c][12] >= STRONG && state.kumi_info[c][0] == 3)
				state.kumi_info[13][33] = state.kumi_info[13][33] + 1;

			if (state.kumi_info[c][3] == 1 && (state.kumi_info[c][0] == 1 || state.kumi_info[c][0] == 2))
				state.kumi_info[13][34] = state.kumi_info[13][34] + 1;

			if (state.kumi_info[c][12] >= STRONG - 1)
				state.kumi_info[13][35] = state.kumi_info[13][35] + 1;

			if (state.kumi_info[c][1] == 1 && state.kumi_info[c][12] >= STRONG - 1 && state.kumi_info[c][3] != 6 && state.kumi_info[c][3] != 1)
				state.kumi_info[13][36] = state.kumi_info[13][36] + 1;
			if (state.kumi_info[c][1] == 1 && state.kumi_info[c][12] >= STRONG - 1 && state.kumi_info[c][3] == 6)
				state.kumi_info[13][37] = state.kumi_info[13][37] + 1;
			if (state.kumi_info[c][1] == 2 && state.kumi_info[c][12] >= STRONG - 1 && state.kumi_info[c][3] != 6 && state.kumi_info[c][3] != 1)
				state.kumi_info[13][38] = state.kumi_info[13][38] + 1;
			if (state.kumi_info[c][1] == 2 && state.kumi_info[c][12] >= STRONG - 1 && state.kumi_info[c][3] == 6)
				state.kumi_info[13][39] = state.kumi_info[13][39] + 1;
			if (state.kumi_info[c][1] == 3 && state.kumi_info[c][12] >= STRONG - 1 && state.kumi_info[c][3] != 6 && state.kumi_info[c][3] != 1 && state.kumi_info[c][0] == 2)
				state.kumi_info[13][40] = state.kumi_info[13][40] + 1;
			if (state.kumi_info[c][1] == 3 && state.kumi_info[c][12] >= STRONG - 1 && state.kumi_info[c][3] == 6 && state.kumi_info[c][0] == 2)
				state.kumi_info[13][41] = state.kumi_info[13][41] + 1;
			if (state.kumi_info[c][1] == 3 && state.kumi_info[c][12] >= STRONG - 1 && state.kumi_info[c][0] == 3)
				state.kumi_info[13][42] = state.kumi_info[13][42] + 1;
			if (state.kumi_info[c][1] == 4 && state.kumi_info[c][12] >= STRONG - 1 && state.kumi_info[c][3] != 6 && state.kumi_info[c][3] != 1 && state.kumi_info[c][0] == 2)
				state.kumi_info[13][43] = state.kumi_info[13][43] + 1;
			if (state.kumi_info[c][1] == 4 && state.kumi_info[c][12] >= STRONG - 1 && state.kumi_info[c][3] == 6 && state.kumi_info[c][0] == 2)
				state.kumi_info[13][44] = state.kumi_info[13][44] + 1;
			if (state.kumi_info[c][1] == 4 && state.kumi_info[c][12] >= STRONG - 1 && state.kumi_info[c][0] == 3)
				state.kumi_info[13][45] = state.kumi_info[13][45] + 1;

			if (state.kumi_info[c][1] == 1 && state.kumi_info[c][3] == 1)
				state.kumi_info[13][46] = state.kumi_info[13][46] + 1;
			if (state.kumi_info[c][1] == 2 && state.kumi_info[c][3] == 1)
				state.kumi_info[13][47] = state.kumi_info[13][47] + 1;
			if (state.kumi_info[c][1] == 3 && state.kumi_info[c][3] == 1 && state.kumi_info[c][0] == 2)
				state.kumi_info[13][48] = state.kumi_info[13][48] + 1;

			if (state.kumi_info[c][12] >= STRONG - 2)
				state.kumi_info[13][49] = state.kumi_info[13][49] + 1;
			if (state.kumi_info[c][12] >= STRONG - 4)
				state.kumi_info[13][50] = state.kumi_info[13][50] + 1;
		}

		if (state.kumi_info[c][1] >= 1 && state.kumi_info[c][10] < STRONG)
			state.kumi_info[14][0] = state.kumi_info[14][0] + 1;
		if (state.kumi_info[c][1] >= 1 && state.kumi_info[c][12] < STRONG)
			state.kumi_info[14][1] = state.kumi_info[14][1] + 1;
		if (state.kumi_info[c][1] >= 1 && state.kumi_info[c][9] < STRONG * state.kumi_info[c][1])
			state.kumi_info[14][2] = state.kumi_info[14][2] + state.kumi_info[c][9];
		if (state.kumi_info[c][1] >= 1 && state.kumi_info[c][11] < STRONG * state.kumi_info[c][1])
			state.kumi_info[14][3] = state.kumi_info[14][3] + state.kumi_info[c][11];
		if (state.kumi_info[c][1] >= 1 && state.kumi_info[c][10] < STRONG)
			state.kumi_info[14][4] = state.kumi_info[14][4] + state.kumi_info[c][10];
		if (state.kumi_info[c][1] >= 1 && state.kumi_info[c][12] < STRONG)
			state.kumi_info[14][5] = state.kumi_info[14][5] + state.kumi_info[c][12];

		if (state.kumi_info[c][1] >= 1 && state.kumi_info[c][10] >= STRONG - 1 || state.kumi_info[c][1] >= 3)
			state.kumi_info[14][10] = state.kumi_info[14][10] + 1;
		if (state.kumi_info[c][1] >= 1 && state.kumi_info[c][10] >= STRONG || state.kumi_info[c][1] >= 3)
			state.kumi_info[14][11] = state.kumi_info[14][11] + 1;
		if (state.kumi_info[c][1] >= 1 && state.kumi_info[c][10] >= STRONG - 2 || state.kumi_info[c][1] >= 3)
			state.kumi_info[14][12] = state.kumi_info[14][12] + 1;
	}

	if (state.kumi_info[13][15] >= 1 && state.kumi_info[13][0] >= 1)
		state.kumi_info[13][17] = state.kumi_info[13][15] / state.kumi_info[13][0];
	if (state.kumi_info[13][16] >= 1 && state.kumi_info[13][0] >= 1)
		state.kumi_info[13][18] = state.kumi_info[13][16] / state.kumi_info[13][0];
	if (state.kumi_info[13][4] >= 1 && state.kumi_info[13][0] >= 1)
		state.kumi_info[13][19] = state.kumi_info[13][4] / state.kumi_info[13][0];
	if (state.kumi_info[13][5] >= 1 && state.kumi_info[13][0] >= 1)
		state.kumi_info[13][20] = state.kumi_info[13][5] / state.kumi_info[13][0];

	if (state.kumi_info[14][4] >= 1 && state.kumi_info[14][0] >= 1)
		state.kumi_info[14][6] = state.kumi_info[14][4] / state.kumi_info[14][0];
	if (state.kumi_info[14][5] >= 1 && state.kumi_info[14][1] >= 1)
		state.kumi_info[14][7] = state.kumi_info[14][5] / state.kumi_info[14][1];
	if (state.kumi_info[14][2] >= 1 && state.kumi_info[14][0] >= 1)
		state.kumi_info[14][8] = state.kumi_info[14][2] / state.kumi_info[14][0];
	if (state.kumi_info[14][3] >= 1 && state.kumi_info[14][1] >= 1)
		state.kumi_info[14][9] = state.kumi_info[14][3] / state.kumi_info[14][1];

	if (state.kumi_info[14][6] == 0)
		state.kumi_info[14][6] = 999;
	if (state.kumi_info[14][7] == 0)
		state.kumi_info[14][7] = 999;
	if (state.kumi_info[14][8] == 0)
		state.kumi_info[14][8] = 999;
	if (state.kumi_info[14][9] == 0)
		state.kumi_info[14][9] = 999;

	state.kumi_info[15][0] = state.kumi_info[13][4];
	kuminumber = min_kumi_info(10);
	if (kuminumber == 0)
		state.kumi_info[15][0] = state.kumi_info[15][0] - state.kumi_info[0][9];
	if (kuminumber == 1)
		state.kumi_info[15][0] = state.kumi_info[15][0] - state.kumi_info[1][9];
	if (kuminumber == 2)
		state.kumi_info[15][0] = state.kumi_info[15][0] - state.kumi_info[2][9];
	if (kuminumber == 3)
		state.kumi_info[15][0] = state.kumi_info[15][0] - state.kumi_info[3][9];
	if (kuminumber == 4)
		state.kumi_info[15][0] = state.kumi_info[15][0] - state.kumi_info[4][9];
	if (kuminumber == 5)
		state.kumi_info[15][0] = state.kumi_info[15][0] - state.kumi_info[5][9];
	if (kuminumber == 6)
		state.kumi_info[15][0] = state.kumi_info[15][0] - state.kumi_info[6][9];
	if (kuminumber == 7)
		state.kumi_info[15][0] = state.kumi_info[15][0] - state.kumi_info[7][9];
	if (kuminumber == 8)
		state.kumi_info[15][0] = state.kumi_info[15][0] - state.kumi_info[8][9];
	if (kuminumber == 9)
		state.kumi_info[15][0] = state.kumi_info[15][0] - state.kumi_info[9][9];
	if (kuminumber == 10)
		state.kumi_info[15][0] = state.kumi_info[15][0] - state.kumi_info[10][9];
	if (kuminumber == 11)
		state.kumi_info[15][0] = state.kumi_info[15][0] - state.kumi_info[11][9];
	if (kuminumber == 12)
		state.kumi_info[15][0] = state.kumi_info[15][0] - state.kumi_info[12][9];
}

int setvalue_single2(int cards[8][15])
{
	// int kaisuu ではない

	int i = 0, j = 0, a = 0, b = 0, value, teisuu = 100, count = 0, eight = 0;
	int value_rev = 0, count_rev = 0, joker = 0;

	//clearkumi_info(kaisuu);

	for (i = 0; i < 4; i++)
	{
		for (j = 1; j < 14; j++)
		{
			if (cards[i][j] == 1)
			{
				a = i;
				b = j;
				if (j == 6)
					eight = eight + 1;
			}
			if (cards[i][j] == 2)
				joker = 1;
		}
	}
	for (i = 0; i < 4; i++)
	{
		for (j = b + 1; j < 14; j++)
		{
			if (state.submitted_cards_plus_hands[i][j] == 0)
			{
				count++;
			}
		}
	}

	//革命用の評価値の設定
	for (i = 0; i < 4; i++)
	{
		for (j = b - 1; j > 0; j--)
		{
			if (state.submitted_cards_plus_hands[i][j] == 0)
			{
				count_rev++;
			}
		}
	}

	if (eight != 0)
	{
		//value=teisuu-count+20;//8は20点加点することにした。
		//if(value>100)value=100;
		value = 100; //2017.06.13
		//value_rev=teisuu-count_rev+20;//8は20点加点することにした。
		//if(value_rev>100)value_rev=100;
		value_rev = 100; //2017.06.13
	}
	else
	{
		if (state.have_s3 == 1)
		{
			value = teisuu - count;
			value_rev = teisuu - count_rev;
		}
		else
		{
			if (state.player_number > 2)
			{
				if (state.submit_s3 == 0)
				{
					if (state.have_s3 == 1)
					{
						value = teisuu - count;
						value_rev = teisuu - count_rev;
					}
					else
					{
						value = teisuu - count - state.joker_risk;
						value_rev = teisuu - count_rev - state.joker_risk;
					}
				}
				else
				{
					value = teisuu - count - state.joker_risk;
					value_rev = teisuu - count_rev - state.joker_risk;
				}
			}
			else
			{
				value = teisuu - count - state.joker_risk;
				value_rev = teisuu - count_rev - state.joker_risk;
			}
		}
	}
	/*
			if(state.submit_s3==0){
				if(state.player_number>2){
					value=teisuu - count;
					value_rev=teisuu-count_rev;
				}else{
					if(state.have_s3==1){
						value=teisuu - count;
						value_rev=teisuu-count_rev;	
					}else{
						value=teisuu - count-state.joker_risk;
						value_rev=teisuu-count_rev-state.joker_risk;
					}
				}
			}else{
				value=teisuu - count-state.joker_risk;
				value_rev=teisuu-count_rev-state.joker_risk;
			}
		*/
	//}
	/*
	state.kumi_info[kaisuu][0]=1;
	state.kumi_info[kaisuu][1]=1;
	state.kumi_info[kaisuu][2]=a;
	state.kumi_info[kaisuu][3]=b;
	state.kumi_info[kaisuu][4]=b;
	state.kumi_info[kaisuu][5]=value;//強さ
	state.kumi_info[kaisuu][6]=value;
	state.kumi_info[kaisuu][7]=value_rev;//強さ
	state.kumi_info[kaisuu][8]=value_rev;
	
	
	if(eight!=0){		
		state.kumi_info[kaisuu][9]=101;
		state.kumi_info[kaisuu][10]=101;
		state.kumi_info[kaisuu][11]=101;
		state.kumi_info[kaisuu][12]=101;
	}else{
		state.kumi_info[kaisuu][9]=value;//流し
		state.kumi_info[kaisuu][10]=value;
		state.kumi_info[kaisuu][11]=value_rev;
		state.kumi_info[kaisuu][12]=value_rev;
	}
	
	state.kumi_info[kaisuu][13]=0;
	state.kumi_info[kaisuu][14]=0;
	*/

	if (cards[4][1] != 0 || cards[0][0] == 2 || cards[0][14] == 2 || joker == 1)
	{
		value = 100 - state.s3_risk;

		if (state.rev == 0)
		{
			clearCards(cards);
			cards[0][14] = 2;
		}
		else
		{
			clearCards(cards);
			cards[0][0] = 2;
		}
		/*
		state.kumi_info[kaisuu][0]=1;
		state.kumi_info[kaisuu][1]=1;
		state.kumi_info[kaisuu][2]=4;//仮
		state.kumi_info[kaisuu][3]=14;//仮
		state.kumi_info[kaisuu][4]=14;
		state.kumi_info[kaisuu][5]=value;
		state.kumi_info[kaisuu][6]=value;
		state.kumi_info[kaisuu][7]=value;
		state.kumi_info[kaisuu][8]=value;	
		state.kumi_info[kaisuu][9]=value;
		state.kumi_info[kaisuu][10]=value;
		state.kumi_info[kaisuu][11]=value;
		state.kumi_info[kaisuu][12]=value;
		state.kumi_info[kaisuu][13]=1;
		state.kumi_info[kaisuu][14]=0;
		*/
	}

	//outputkumi_info(kaisuu);
	if (state.rev == 0)
	{
		return value;
	}
	else
	{
		return value_rev;
	}
}

int setvalue_pair2(int cards[8][15])
{

	int i = 0, j = 0, a = 0, b = 0, value = 0, teisuu = 100, count = 0, maisuu = 0, suitsum = 0, eight = 0;
	int value_rev = 0, count_rev = 0;
	int new_value = 0, new_value_rev = 0;

	//suitを1,2,4,8の4つにわけて判断することにする。

	//clearkumi_info(kaisuu);

	maisuu = qtyOfCards(cards);

	for (i = 0; i < 5; i++)
	{
		for (j = 1; j < 14; j++)
		{
			if (cards[i][j] == 1 || cards[i][j] == 2)
			{
				a = i;
				b = j;

				if (i == 0)
					suitsum = suitsum + 1; //マークを1,2,4,8の組み合わせで表すことにした。
				if (i == 1)
					suitsum = suitsum + 2;
				if (i == 2)
					suitsum = suitsum + 4;
				if (i == 3)
					suitsum = suitsum + 8;

				if (j == 6)
					eight = eight + 1;
				//if(cards[i][j]==2)state.kumi_info[kaisuu][13]=1;
			}
		}
	}

	if (maisuu == 2)
	{
		for (j = b + 1; j < 14; j++)
		{
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0)
				count++;
			if (state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count++;
			if (state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count++;
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
			if (state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
		}
	}

	if (maisuu == 3)
	{
		for (j = b + 1; j < 14; j++)
		{
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count++;
			if (state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
		}
	}

	if (maisuu == 4)
	{
		for (j = b - 1; j > 0; j--)
		{ //革命
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
		}
	}

	//革命用の評価値の設定

	if (maisuu == 2)
	{
		for (j = b - 1; j > 0; j--)
		{
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0)
				count_rev++;
			if (state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count_rev++;
			if (state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count_rev++;
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
			if (state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
		}
	}

	if (maisuu == 3)
	{
		for (j = b - 1; j > 0; j--)
		{
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count_rev++;
			if (state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
		}
	}

	if (maisuu == 4)
	{
		for (j = b + 1; j < 14; j++)
		{ //革命
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
		}
	}
	//ここまで

	if (eight != 0)
	{
		/*
		value=teisuu * maisuu - count+20;//8は20点加点することにした。
		if(value>(100*maisuu))value=100*maisuu;
		*/
		value = 100 * maisuu;
		new_value = 100;
		/*
		value_rev=teisuu * maisuu - count_rev+20;//8は20点加点することにした。
		if(value_rev>(100*maisuu))value_rev=100*maisuu;
		*/
		value_rev = 100 * maisuu;
		new_value_rev = 100;
	}
	else
	{
		//value=teisuu * maisuu - count-state.joker_risk+state.player_number_point*maisuu;
		//value_rev=teisuu * maisuu - count_rev-state.joker_risk+state.player_number_point*maisuu;

		value = teisuu * maisuu - count;
		value_rev = teisuu * maisuu - count_rev;

		new_value = value / maisuu;
		new_value_rev = value_rev / maisuu;
	}
	/*
	state.kumi_info[kaisuu][0]=2;
	state.kumi_info[kaisuu][1]=maisuu;
	state.kumi_info[kaisuu][2]=suitsum;
	state.kumi_info[kaisuu][3]=b;
	state.kumi_info[kaisuu][4]=b;
	state.kumi_info[kaisuu][5]=value;
	state.kumi_info[kaisuu][6]=value/maisuu;
	state.kumi_info[kaisuu][7]=value_rev;
	state.kumi_info[kaisuu][8]=value_rev/maisuu;
	
	if(eight!=0){
		state.kumi_info[kaisuu][9]=100*maisuu+1;
		state.kumi_info[kaisuu][10]=101;
		state.kumi_info[kaisuu][11]=100*maisuu+1;
		state.kumi_info[kaisuu][12]=101;
	}else{
		state.kumi_info[kaisuu][9]=value;//流し
		state.kumi_info[kaisuu][10]=value/maisuu;
		state.kumi_info[kaisuu][11]=value_rev;//流し
		state.kumi_info[kaisuu][12]=value_rev/maisuu;
	}
	
	//state.kumi_info[kaisuu][13]=0;//仮
	if(maisuu>=4){
	state.kumi_info[kaisuu][14]=1;
	}
	
	//outputkumi_info(kaisuu);
	*/

	if (state.rev == 0)
	{
		//return value;
		return new_value;
	}
	else
	{
		//return value_rev;
		return new_value_rev;
	}
}

int setvalue_kaidan2(int cards[8][15])
{
	int i = 0, j = 0, a = 0, b = 0, value = 0, teisuu = 100, count = 0, maisuu = 0, eight = 0;
	int c = 0, d = 0, value_rev = 0, count_rev = 0;
	int new_value = 0, new_value_rev = 0;

	//clearkumi_info(kaisuu);

	maisuu = qtyOfCards(cards);

	for (i = 0; i < 5; i++)
	{
		for (j = 1; j < 14; j++)
		{
			if (cards[i][j] == 1 || cards[i][j] == 2)
			{
				a = i;
				b = j;

				if (j == 6)
					eight = eight + 1;
				//if(cards[i][j]==2)state.kumi_info[kaisuu][13]=1;
			}
		}
	}

	if (maisuu == 3)
	{
		for (i = 0; i < 4; i++)
		{
			for (j = b + 1; j < 15 - maisuu; j++)
			{
				if (state.submitted_cards_plus_hands[i][j] == 0 && state.submitted_cards_plus_hands[i][j + 1] == 0 && state.submitted_cards_plus_hands[i][j + 2] == 0)
					count++;
			}
		}
	}

	if (maisuu == 4)
	{
		for (i = 0; i < 4; i++)
		{
			for (j = b + 1; j < 15 - maisuu; j++)
			{
				if (state.submitted_cards_plus_hands[i][j] == 0 && state.submitted_cards_plus_hands[i][j + 1] == 0 && state.submitted_cards_plus_hands[i][j + 2] == 0 && state.submitted_cards_plus_hands[i][j + 3] == 0)
					count++;
			}
		}
	}

	if (maisuu == 5)
	{
		for (i = 0; i < 4; i++)
		{
			for (j = b + 1; j < 15 - maisuu; j++)
			{
				if (state.submitted_cards_plus_hands[i][j] == 0 && state.submitted_cards_plus_hands[i][j + 1] == 0 && state.submitted_cards_plus_hands[i][j + 2] == 0 && state.submitted_cards_plus_hands[i][j + 3] == 0 && state.submitted_cards_plus_hands[i][j + 4] == 0)
					count++;
			}
		}
	}

	//革命用
	for (i = 0; i < 5; i++)
	{
		for (j = 13; j > 0; j--)
		{
			if (cards[i][j] == 1 || cards[i][j] == 2)
			{
				c = i;
				d = j;
			}
		}
	}

	if (maisuu == 3)
	{
		for (i = 0; i < 4; i++)
		{
			for (j = d - 1; j > maisuu - 1; j--)
			{
				if (state.submitted_cards_plus_hands[i][j] == 0 && state.submitted_cards_plus_hands[i][j - 1] == 0 && state.submitted_cards_plus_hands[i][j - 2] == 0)
					count_rev++;
			}
		}
	}

	if (maisuu == 4)
	{
		for (i = 0; i < 4; i++)
		{
			for (j = d - 1; j > maisuu - 1; j--)
			{
				if (state.submitted_cards_plus_hands[i][j] == 0 && state.submitted_cards_plus_hands[i][j - 1] == 0 && state.submitted_cards_plus_hands[i][j - 2] == 0 && state.submitted_cards_plus_hands[i][j - 3] == 0)
					count_rev++;
			}
		}
	}

	if (maisuu == 5)
	{
		for (i = 0; i < 4; i++)
		{
			for (j = d - 1; j > maisuu - 1; j--)
			{
				if (state.submitted_cards_plus_hands[i][j] == 0 && state.submitted_cards_plus_hands[i][j - 1] == 0 && state.submitted_cards_plus_hands[i][j - 2] == 0 && state.submitted_cards_plus_hands[i][j - 3] == 0 && state.submitted_cards_plus_hands[i][j - 4] == 0)
					count_rev++;
			}
		}
	}
	//ここまで

	if (eight != 0)
	{
		/*
		value=teisuu * maisuu - count+20;//8は20点加点することにした。
		if(value>(100*maisuu))value=100*maisuu;
		*/
		value = 100 * maisuu;
		new_value = 100;
		/*
		value_rev=teisuu * maisuu - count_rev+20;//8は20点加点することにした。
		if(value_rev>(100*maisuu))value_rev=100*maisuu;
		*/
		value_rev = 100 * maisuu;
		new_value_rev = 100;
	}
	else
	{
		//value=teisuu * maisuu - count-state.joker_risk+state.player_number_point*maisuu;
		//value_rev=teisuu * maisuu - count_rev-state.joker_risk+state.player_number_point*maisuu;

		value = teisuu * maisuu - count;
		value_rev = teisuu * maisuu - count_rev;

		new_value = value / maisuu;
		new_value_rev = value_rev / maisuu;
	}
	/*
	state.kumi_info[kaisuu][0]=3;
	state.kumi_info[kaisuu][1]=maisuu;
	state.kumi_info[kaisuu][2]=a;
	state.kumi_info[kaisuu][3]=b-maisuu+1;
	state.kumi_info[kaisuu][4]=b;
	state.kumi_info[kaisuu][5]=value;
	state.kumi_info[kaisuu][6]=value/maisuu;
	state.kumi_info[kaisuu][7]=value_rev;
	state.kumi_info[kaisuu][8]=value_rev/maisuu;
	
	if(eight!=0){
		state.kumi_info[kaisuu][9]=100*maisuu+1;
		state.kumi_info[kaisuu][10]=101;
		state.kumi_info[kaisuu][11]=100*maisuu+1;
		state.kumi_info[kaisuu][12]=101;
	}else{
		state.kumi_info[kaisuu][9]=value;//流し
		state.kumi_info[kaisuu][10]=value/maisuu;
		state.kumi_info[kaisuu][11]=value_rev;//流し
		state.kumi_info[kaisuu][12]=value_rev/maisuu;
	}
	
	//state.kumi_info[kaisuu][13]=0;//仮
	if(maisuu>=5){
	state.kumi_info[kaisuu][14]=1;
	}
	
	//outputkumi_info(kaisuu);
	*/
	if (state.rev == 0)
	{
		//return value;
		return new_value;
	}
	else
	{
		//return value_rev;
		return new_value_rev;
	}
}

int setvalue_single_shibari(int cards[8][15], int mark)
{

	int i = 0, j = 0, a = 0, b = 0, value = 0, teisuu = 100, count = 0, eight = 0;
	int value_rev = 0, count_rev = 0, joker = 0;
	int new_value = 0, new_value_rev = 0;

	//clearkumi_info(kaisuu);

	for (i = 0; i < 4; i++)
	{
		for (j = 1; j < 14; j++)
		{
			if (cards[i][j] == 1)
			{
				a = i;
				b = j;
				if (j == 6)
					eight = eight + 1;
			}
			if (cards[i][j] == 2)
				joker = 1;
		}
	}
	//for(i=0;i<4;i++){
	for (j = b + 1; j < 14; j++)
	{
		if (state.submitted_cards_plus_hands[mark][j] == 0)
		{
			count++;
		}
	}
	//	}

	//革命用の評価値の設定
	//for(i=0;i<4;i++){
	for (j = b - 1; j > 0; j--)
	{
		if (state.submitted_cards_plus_hands[mark][j] == 0)
		{
			count_rev++;
		}
	}
	//	}

	if (eight != 0)
	{
		//value=teisuu-count+20;//8は20点加点することにした。
		//if(value>100)value=100;
		value = 100; //2017.06.13
		//value_rev=teisuu-count_rev+20;//8は20点加点することにした。
		//if(value_rev>100)value_rev=100;
		value_rev = 100; //2017.06.13
	}
	else
	{

		if (state.have_s3 == 1)
		{
			value = teisuu - count;
			value_rev = teisuu - count_rev;
		}
		else
		{
			if (state.player_number > 2)
			{
				if (state.submit_s3 == 0)
				{
					if (state.have_s3 == 1)
					{
						value = teisuu - count;
						value_rev = teisuu - count_rev;
					}
					else
					{
						value = teisuu - count - state.joker_risk;
						value_rev = teisuu - count_rev - state.joker_risk;
					}
				}
				else
				{
					value = teisuu - count - state.joker_risk;
					value_rev = teisuu - count_rev - state.joker_risk;
				}
			}
			else
			{
				value = teisuu - count - state.joker_risk;
				value_rev = teisuu - count_rev - state.joker_risk;
			}
		}

		/*
		if(state.submit_s3==0){
			if(state.player_number>1){
				value=teisuu - count;
				value_rev=teisuu-count_rev;
			}else{
				if(state.have_s3==1){
					value=teisuu - count;
					value_rev=teisuu-count_rev;	
				}else{
					value=teisuu - count-state.joker_risk;
					value_rev=teisuu-count_rev-state.joker_risk;
				}
			}
		}else{
			value=teisuu - count-state.joker_risk;
			value_rev=teisuu-count_rev-state.joker_risk;
		}
		*/
	}

	/*
	state.kumi_info[kaisuu][0]=1;
	state.kumi_info[kaisuu][1]=1;
	state.kumi_info[kaisuu][2]=a;
	state.kumi_info[kaisuu][3]=b;
	state.kumi_info[kaisuu][4]=b;
	state.kumi_info[kaisuu][5]=value;//強さ
	state.kumi_info[kaisuu][6]=value;
	state.kumi_info[kaisuu][7]=value_rev;//強さ
	state.kumi_info[kaisuu][8]=value_rev;
	
	
	if(eight!=0){		
		state.kumi_info[kaisuu][9]=101;
		state.kumi_info[kaisuu][10]=101;
		state.kumi_info[kaisuu][11]=101;
		state.kumi_info[kaisuu][12]=101;
	}else{
		state.kumi_info[kaisuu][9]=value;//流し
		state.kumi_info[kaisuu][10]=value;
		state.kumi_info[kaisuu][11]=value_rev;
		state.kumi_info[kaisuu][12]=value_rev;
	}
	
	state.kumi_info[kaisuu][13]=0;
	state.kumi_info[kaisuu][14]=0;
	*/

	if (cards[4][1] != 0 || cards[0][0] == 2 || cards[0][14] == 2 || joker == 1)
	{
		value = 100 - state.s3_risk;

		if (state.rev == 0)
		{
			clearCards(cards);
			cards[0][14] = 2;
		}
		else
		{
			clearCards(cards);
			cards[0][0] = 2;
		}
		/*
		state.kumi_info[kaisuu][0]=1;
		state.kumi_info[kaisuu][1]=1;
		state.kumi_info[kaisuu][2]=4;//仮
		state.kumi_info[kaisuu][3]=14;//仮
		state.kumi_info[kaisuu][4]=14;
		state.kumi_info[kaisuu][5]=value;
		state.kumi_info[kaisuu][6]=value;
		state.kumi_info[kaisuu][7]=value;
		state.kumi_info[kaisuu][8]=value;	
		state.kumi_info[kaisuu][9]=value;
		state.kumi_info[kaisuu][10]=value;
		state.kumi_info[kaisuu][11]=value;
		state.kumi_info[kaisuu][12]=value;
		state.kumi_info[kaisuu][13]=1;
		state.kumi_info[kaisuu][14]=0;
		*/
	}

	//outputkumi_info(kaisuu);
	if (state.rev == 0)
	{
		return value;
	}
	else
	{
		return value_rev;
	}
}

int setvalue_pair_shibari(int cards[8][15], int mark)
{

	int i = 0, j = 0, a = 0, b = 0, value = 0, teisuu = 100, count = 0, maisuu = 0, suitsum = 0, eight = 0;
	int value_rev = 0, count_rev = 0;
	int new_value = 0, new_value_rev = 0;

	//suitを1,2,4,8の4つにわけて判断することにする。

	//clearkumi_info(kaisuu);

	maisuu = qtyOfCards(cards);

	for (i = 0; i < 5; i++)
	{
		for (j = 1; j < 14; j++)
		{
			if (cards[i][j] == 1 || cards[i][j] == 2)
			{
				a = i;
				b = j;

				//if(i==0)suitsum=suitsum+1;//マークを1,2,4,8の組み合わせで表すことにした。
				//if(i==1)suitsum=suitsum+2;
				//if(i==2)suitsum=suitsum+4;
				//if(i==3)suitsum=suitsum+8;

				if (j == 6)
					eight = eight + 1;
				//if(cards[i][j]==2)state.kumi_info[kaisuu][13]=1;
			}
		}
	}

	if (maisuu == 2)
	{
		for (j = b + 1; j < 14; j++)
		{
			if (mark == 3 && state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0)
				count++;
			if (mark == 6 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count++;
			if (mark == 12 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
			if (mark == 5 && state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count++;
			if (mark == 9 && state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
			if (mark == 10 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
		}
	}

	if (maisuu == 3)
	{
		for (j = b + 1; j < 14; j++)
		{
			if (mark == 7 && state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count++;
			if (mark == 14 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
			if (mark == 11 && state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
			if (mark == 13 && state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
		}
	}

	if (maisuu == 4)
	{
		for (j = b - 1; j > 0; j--)
		{ //革命
			if (mark == 15 && state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
		}
	}

	//革命用の評価値の設定

	if (maisuu == 2)
	{
		for (j = b - 1; j > 0; j--)
		{
			if (mark == 3 && state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0)
				count_rev++;
			if (mark == 6 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count_rev++;
			if (mark == 12 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
			if (mark == 5 && state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count_rev++;
			if (mark == 9 && state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
			if (mark == 10 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
		}
	}

	if (maisuu == 3)
	{
		for (j = b - 1; j > 0; j--)
		{
			if (mark == 7 && state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count_rev++;
			if (mark == 14 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
			if (mark == 11 && state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
			if (mark == 13 && state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
		}
	}

	if (maisuu == 4)
	{
		for (j = b + 1; j < 14; j++)
		{ //革命
			if (mark == 15 && state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
		}
	}
	//ここまで

	if (eight != 0)
	{
		/*
		value=teisuu * maisuu - count+20;//8は20点加点することにした。
		if(value>(100*maisuu))value=100*maisuu;
		*/
		value = 100 * maisuu;
		new_value = 100;
		/*
		value_rev=teisuu * maisuu - count_rev+20;//8は20点加点することにした。
		if(value_rev>(100*maisuu))value_rev=100*maisuu;
		*/
		value_rev = 100 * maisuu;
		new_value_rev = 100;
	}
	else
	{
		//value=teisuu * maisuu - count-state.joker_risk+state.player_number_point*maisuu;
		//value_rev=teisuu * maisuu - count_rev-state.joker_risk+state.player_number_point*maisuu;
		value = teisuu * maisuu - count;
		value_rev = teisuu * maisuu - count_rev;

		new_value = value / maisuu;
		new_value_rev = value_rev / maisuu;
	}
	/*
	state.kumi_info[kaisuu][0]=2;
	state.kumi_info[kaisuu][1]=maisuu;
	state.kumi_info[kaisuu][2]=suitsum;
	state.kumi_info[kaisuu][3]=b;
	state.kumi_info[kaisuu][4]=b;
	state.kumi_info[kaisuu][5]=value;
	state.kumi_info[kaisuu][6]=value/maisuu;
	state.kumi_info[kaisuu][7]=value_rev;
	state.kumi_info[kaisuu][8]=value_rev/maisuu;
	
	if(eight!=0){
		state.kumi_info[kaisuu][9]=100*maisuu+1;
		state.kumi_info[kaisuu][10]=101;
		state.kumi_info[kaisuu][11]=100*maisuu+1;
		state.kumi_info[kaisuu][12]=101;
	}else{
		state.kumi_info[kaisuu][9]=value;//流し
		state.kumi_info[kaisuu][10]=value/maisuu;
		state.kumi_info[kaisuu][11]=value_rev;//流し
		state.kumi_info[kaisuu][12]=value_rev/maisuu;
	}
	
	//state.kumi_info[kaisuu][13]=0;//仮
	if(maisuu>=4){
	state.kumi_info[kaisuu][14]=1;
	}
	
	//outputkumi_info(kaisuu);
	
	*/
	if (state.rev == 0)
	{
		//return value;
		return new_value;
	}
	else
	{
		//return value_rev;
		return new_value_rev;
	}
}

int setvalue_kaidan_shibari(int cards[8][15], int mark)
{

	int i = 0, j = 0, a = 0, b = 0, value = 0, teisuu = 100, count = 0, maisuu = 0, eight = 0;
	int c = 0, d = 0, value_rev = 0, count_rev = 0;
	int new_value = 0, new_value_rev = 0;

	//clearkumi_info(kaisuu);

	maisuu = qtyOfCards(cards);

	for (i = 0; i < 5; i++)
	{
		for (j = 1; j < 14; j++)
		{
			if (cards[i][j] == 1 || cards[i][j] == 2)
			{
				a = i;
				b = j;

				if (j == 6)
					eight = eight + 1;
				//if(cards[i][j]==2)state.kumi_info[kaisuu][13]=1;
			}
		}
	}

	if (maisuu == 3)
	{
		//for(i=0;i<4;i++){
		for (j = b + 1; j < 15 - maisuu; j++)
		{
			if (state.submitted_cards_plus_hands[mark][j] == 0 && state.submitted_cards_plus_hands[mark][j + 1] == 0 && state.submitted_cards_plus_hands[mark][j + 2] == 0)
				count++;
		}
		//}
	}

	if (maisuu == 4)
	{
		//for(i=0;i<4;i++){
		for (j = b + 1; j < 15 - maisuu; j++)
		{
			if (state.submitted_cards_plus_hands[mark][j] == 0 && state.submitted_cards_plus_hands[mark][j + 1] == 0 && state.submitted_cards_plus_hands[mark][j + 2] == 0 && state.submitted_cards_plus_hands[mark][j + 3] == 0)
				count++;
		}
		//}
	}

	if (maisuu == 5)
	{
		//for(i=0;i<4;i++){
		for (j = b + 1; j < 15 - maisuu; j++)
		{
			if (state.submitted_cards_plus_hands[mark][j] == 0 && state.submitted_cards_plus_hands[mark][j + 1] == 0 && state.submitted_cards_plus_hands[mark][j + 2] == 0 && state.submitted_cards_plus_hands[mark][j + 3] == 0 && state.submitted_cards_plus_hands[mark][j + 4] == 0)
				count++;
		}
		//}
	}

	//革命用
	for (i = 0; i < 5; i++)
	{
		for (j = 13; j > 0; j--)
		{
			if (cards[i][j] == 1 || cards[i][j] == 2)
			{
				c = i;
				d = j;
			}
		}
	}

	if (maisuu == 3)
	{
		//for(i=0;i<4;i++){
		for (j = d - 1; j > maisuu - 1; j--)
		{
			if (state.submitted_cards_plus_hands[mark][j] == 0 && state.submitted_cards_plus_hands[mark][j - 1] == 0 && state.submitted_cards_plus_hands[mark][j - 2] == 0)
				count_rev++;
		}
		//}
	}

	if (maisuu == 4)
	{
		//for(i=0;i<4;i++){
		for (j = d - 1; j > maisuu - 1; j--)
		{
			if (state.submitted_cards_plus_hands[mark][j] == 0 && state.submitted_cards_plus_hands[mark][j - 1] == 0 && state.submitted_cards_plus_hands[mark][j - 2] == 0 && state.submitted_cards_plus_hands[mark][j - 3] == 0)
				count_rev++;
		}
		//}
	}

	if (maisuu == 5)
	{
		//for(i=0;i<4;i++){
		for (j = d - 1; j > maisuu - 1; j--)
		{
			if (state.submitted_cards_plus_hands[mark][j] == 0 && state.submitted_cards_plus_hands[mark][j - 1] == 0 && state.submitted_cards_plus_hands[mark][j - 2] == 0 && state.submitted_cards_plus_hands[mark][j - 3] == 0 && state.submitted_cards_plus_hands[mark][j - 4] == 0)
				count_rev++;
		}
		//}
	}
	//ここまで

	if (eight != 0)
	{
		/*
		value=teisuu * maisuu - count+20;//8は20点加点することにした。
		if(value>(100*maisuu))value=100*maisuu;
		*/
		value = 100 * maisuu;
		new_value = 100;
		/*
		value_rev=teisuu * maisuu - count_rev+20;//8は20点加点することにした。
		if(value_rev>(100*maisuu))value_rev=100*maisuu;
		*/
		value_rev = 100 * maisuu;
		new_value_rev = 100;
	}
	else
	{
		//value=teisuu * maisuu - count-state.joker_risk+state.player_number_point*maisuu;
		//value_rev=teisuu * maisuu - count_rev-state.joker_risk+state.player_number_point*maisuu;

		value = teisuu * maisuu - count;
		value_rev = teisuu * maisuu - count_rev;

		//printf("state.player_number_point  %d\n",state.player_number_point);
		//printf("maisuu  %d\n",maisuu);

		new_value = value / maisuu;
		new_value_rev = value_rev / maisuu;

		//printf("new_value  %d\n",new_value);
	}

	/*
	state.kumi_info[kaisuu][0]=3;
	state.kumi_info[kaisuu][1]=maisuu;
	state.kumi_info[kaisuu][2]=a;
	state.kumi_info[kaisuu][3]=b-maisuu+1;
	state.kumi_info[kaisuu][4]=b;
	state.kumi_info[kaisuu][5]=value;
	state.kumi_info[kaisuu][6]=value/maisuu;
	state.kumi_info[kaisuu][7]=value_rev;
	state.kumi_info[kaisuu][8]=value_rev/maisuu;
	
	if(eight!=0){
		state.kumi_info[kaisuu][9]=100*maisuu+1;
		state.kumi_info[kaisuu][10]=101;
		state.kumi_info[kaisuu][11]=100*maisuu+1;
		state.kumi_info[kaisuu][12]=101;
	}else{
		state.kumi_info[kaisuu][9]=value;//流し
		state.kumi_info[kaisuu][10]=value/maisuu;
		state.kumi_info[kaisuu][11]=value_rev;//流し
		state.kumi_info[kaisuu][12]=value_rev/maisuu;
	}
	
	//state.kumi_info[kaisuu][13]=0;//仮
	if(maisuu>=5){
	state.kumi_info[kaisuu][14]=1;
	}
	
	//outputkumi_info(kaisuu);
	*/

	if (state.rev == 0)
	{
		//return value;
		return new_value;
	}
	else
	{
		//return value_rev;
		return new_value_rev;
	}
}

int setvalue_pair3(int cards[8][15])
{

	int i = 0, j = 0, a = 0, b = 0, value = 0, teisuu = 100, count = 0, maisuu = 0, suitsum = 0, eight = 0;
	int value_rev = 0, count_rev = 0;
	int new_value = 0, new_value_rev = 0;

	//suitを1,2,4,8の4つにわけて判断することにする。

	//clearkumi_info(kaisuu);

	maisuu = qtyOfCards(cards);

	for (i = 0; i < 5; i++)
	{
		for (j = 1; j < 14; j++)
		{
			if (cards[i][j] == 1 || cards[i][j] == 2)
			{
				a = i;
				b = j;

				if (i == 0)
					suitsum = suitsum + 1; //マークを1,2,4,8の組み合わせで表すことにした。
				if (i == 1)
					suitsum = suitsum + 2;
				if (i == 2)
					suitsum = suitsum + 4;
				if (i == 3)
					suitsum = suitsum + 8;

				if (j == 6)
					eight = eight + 1;
				//if(cards[i][j]==2)state.kumi_info[kaisuu][13]=1;
			}
		}
	}

	if (maisuu == 2)
	{
		for (j = b + 1; j < 14; j++)
		{
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0)
				count++;
			if (state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count++;
			if (state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count++;
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
			if (state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
		}
	}

	if (maisuu == 3)
	{
		for (j = b + 1; j < 14; j++)
		{
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count++;
			if (state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
		}
	}

	if (maisuu == 4)
	{
		for (j = b - 1; j > 0; j--)
		{ //革命
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
		}
	}

	//革命用の評価値の設定

	if (maisuu == 2)
	{
		for (j = b - 1; j > 0; j--)
		{
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0)
				count_rev++;
			if (state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count_rev++;
			if (state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count_rev++;
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
			if (state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
		}
	}

	if (maisuu == 3)
	{
		for (j = b - 1; j > 0; j--)
		{
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count_rev++;
			if (state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
		}
	}

	if (maisuu == 4)
	{
		for (j = b + 1; j < 14; j++)
		{ //革命
			if (state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
		}
	}
	//ここまで

	if (eight != 0)
	{
		/*
		value=teisuu * maisuu - count+20;//8は20点加点することにした。
		if(value>(100*maisuu))value=100*maisuu;
		*/
		value = 100 * maisuu;
		new_value = 100;
		/*
		value_rev=teisuu * maisuu - count_rev+20;//8は20点加点することにした。
		if(value_rev>(100*maisuu))value_rev=100*maisuu;
		*/
		value_rev = 100 * maisuu;
		new_value_rev = 100;
	}
	else
	{
		//value=teisuu * maisuu - count-state.joker_risk+state.player_number_point*maisuu;
		//value_rev=teisuu * maisuu - count_rev-state.joker_risk+state.player_number_point*maisuu;

		value = teisuu * maisuu - count;
		value_rev = teisuu * maisuu - count_rev;

		//new_value=value/maisuuk;
		//new_value_rev=value_rev/maisuu;
	}
	/*
	state.kumi_info[kaisuu][0]=2;
	state.kumi_info[kaisuu][1]=maisuu;
	state.kumi_info[kaisuu][2]=suitsum;
	state.kumi_info[kaisuu][3]=b;
	state.kumi_info[kaisuu][4]=b;
	state.kumi_info[kaisuu][5]=value;
	state.kumi_info[kaisuu][6]=value/maisuu;
	state.kumi_info[kaisuu][7]=value_rev;
	state.kumi_info[kaisuu][8]=value_rev/maisuu;
	
	if(eight!=0){
		state.kumi_info[kaisuu][9]=100*maisuu+1;
		state.kumi_info[kaisuu][10]=101;
		state.kumi_info[kaisuu][11]=100*maisuu+1;
		state.kumi_info[kaisuu][12]=101;
	}else{
		state.kumi_info[kaisuu][9]=value;//流し
		state.kumi_info[kaisuu][10]=value/maisuu;
		state.kumi_info[kaisuu][11]=value_rev;//流し
		state.kumi_info[kaisuu][12]=value_rev/maisuu;
	}
	
	//state.kumi_info[kaisuu][13]=0;//仮
	if(maisuu>=4){
	state.kumi_info[kaisuu][14]=1;
	}
	
	//outputkumi_info(kaisuu);
	*/

	if (state.rev == 0)
	{
		return value;
		//return new_value;
	}
	else
	{
		return value_rev;
		//return new_value_rev;
	}
}

int setvalue_kaidan3(int cards[8][15])
{
	int i = 0, j = 0, a = 0, b = 0, value = 0, teisuu = 100, count = 0, maisuu = 0, eight = 0;
	int c = 0, d = 0, value_rev = 0, count_rev = 0;
	int new_value = 0, new_value_rev = 0;

	//clearkumi_info(kaisuu);

	maisuu = qtyOfCards(cards);

	for (i = 0; i < 5; i++)
	{
		for (j = 1; j < 14; j++)
		{
			if (cards[i][j] == 1 || cards[i][j] == 2)
			{
				a = i;
				b = j;

				if (j == 6)
					eight = eight + 1;
				//if(cards[i][j]==2)state.kumi_info[kaisuu][13]=1;
			}
		}
	}

	if (maisuu == 3)
	{
		for (i = 0; i < 4; i++)
		{
			for (j = b + 1; j < 15 - maisuu; j++)
			{
				if (state.submitted_cards_plus_hands[i][j] == 0 && state.submitted_cards_plus_hands[i][j + 1] == 0 && state.submitted_cards_plus_hands[i][j + 2] == 0)
					count++;
			}
		}
	}

	if (maisuu == 4)
	{
		for (i = 0; i < 4; i++)
		{
			for (j = b + 1; j < 15 - maisuu; j++)
			{
				if (state.submitted_cards_plus_hands[i][j] == 0 && state.submitted_cards_plus_hands[i][j + 1] == 0 && state.submitted_cards_plus_hands[i][j + 2] == 0 && state.submitted_cards_plus_hands[i][j + 3] == 0)
					count++;
			}
		}
	}

	if (maisuu == 5)
	{
		for (i = 0; i < 4; i++)
		{
			for (j = b + 1; j < 15 - maisuu; j++)
			{
				if (state.submitted_cards_plus_hands[i][j] == 0 && state.submitted_cards_plus_hands[i][j + 1] == 0 && state.submitted_cards_plus_hands[i][j + 2] == 0 && state.submitted_cards_plus_hands[i][j + 3] == 0 && state.submitted_cards_plus_hands[i][j + 4] == 0)
					count++;
			}
		}
	}

	//革命用
	for (i = 0; i < 5; i++)
	{
		for (j = 13; j > 0; j--)
		{
			if (cards[i][j] == 1 || cards[i][j] == 2)
			{
				c = i;
				d = j;
			}
		}
	}

	if (maisuu == 3)
	{
		for (i = 0; i < 4; i++)
		{
			for (j = d - 1; j > maisuu - 1; j--)
			{
				if (state.submitted_cards_plus_hands[i][j] == 0 && state.submitted_cards_plus_hands[i][j - 1] == 0 && state.submitted_cards_plus_hands[i][j - 2] == 0)
					count_rev++;
			}
		}
	}

	if (maisuu == 4)
	{
		for (i = 0; i < 4; i++)
		{
			for (j = d - 1; j > maisuu - 1; j--)
			{
				if (state.submitted_cards_plus_hands[i][j] == 0 && state.submitted_cards_plus_hands[i][j - 1] == 0 && state.submitted_cards_plus_hands[i][j - 2] == 0 && state.submitted_cards_plus_hands[i][j - 3] == 0)
					count_rev++;
			}
		}
	}

	if (maisuu == 5)
	{
		for (i = 0; i < 4; i++)
		{
			for (j = d - 1; j > maisuu - 1; j--)
			{
				if (state.submitted_cards_plus_hands[i][j] == 0 && state.submitted_cards_plus_hands[i][j - 1] == 0 && state.submitted_cards_plus_hands[i][j - 2] == 0 && state.submitted_cards_plus_hands[i][j - 3] == 0 && state.submitted_cards_plus_hands[i][j - 4] == 0)
					count_rev++;
			}
		}
	}
	//ここまで

	if (eight != 0)
	{
		/*
		value=teisuu * maisuu - count+20;//8は20点加点することにした。
		if(value>(100*maisuu))value=100*maisuu;
		*/
		value = 100 * maisuu;
		new_value = 100;
		/*
		value_rev=teisuu * maisuu - count_rev+20;//8は20点加点することにした。
		if(value_rev>(100*maisuu))value_rev=100*maisuu;
		*/
		value_rev = 100 * maisuu;
		new_value_rev = 100;
	}
	else
	{
		//value=teisuu * maisuu - count -state.joker_risk+state.player_number_point*maisuu;
		//value_rev=teisuu * maisuu - count_rev-state.joker_risk+state.player_number_point*maisuu;

		value = teisuu * maisuu - count;
		value_rev = teisuu * maisuu - count_rev;

		//new_value=value/maisuu;
		//new_value_rev=value_rev/maisuu ;
	}
	/*
	state.kumi_info[kaisuu][0]=3;
	state.kumi_info[kaisuu][1]=maisuu;
	state.kumi_info[kaisuu][2]=a;
	state.kumi_info[kaisuu][3]=b-maisuu+1;
	state.kumi_info[kaisuu][4]=b;
	state.kumi_info[kaisuu][5]=value;
	state.kumi_info[kaisuu][6]=value/maisuu;
	state.kumi_info[kaisuu][7]=value_rev;
	state.kumi_info[kaisuu][8]=value_rev/maisuu;
	
	if(eight!=0){
		state.kumi_info[kaisuu][9]=100*maisuu+1;
		state.kumi_info[kaisuu][10]=101;
		state.kumi_info[kaisuu][11]=100*maisuu+1;
		state.kumi_info[kaisuu][12]=101;
	}else{
		state.kumi_info[kaisuu][9]=value;//流し
		state.kumi_info[kaisuu][10]=value/maisuu;
		state.kumi_info[kaisuu][11]=value_rev;//流し
		state.kumi_info[kaisuu][12]=value_rev/maisuu;
	}
	
	//state.kumi_info[kaisuu][13]=0;//仮
	if(maisuu>=5){
	state.kumi_info[kaisuu][14]=1;
	}
	
	//outputkumi_info(kaisuu);
	*/
	if (state.rev == 0)
	{
		return value;
		//return new_value;
	}
	else
	{
		return value_rev;
		//return new_value_rev;
	}
}

int setvalue_pair_shibari3(int cards[8][15], int mark)
{

	int i = 0, j = 0, a = 0, b = 0, value = 0, teisuu = 100, count = 0, maisuu = 0, suitsum = 0, eight = 0;
	int value_rev = 0, count_rev = 0;
	int new_value = 0, new_value_rev = 0;

	//suitを1,2,4,8の4つにわけて判断することにする。

	//clearkumi_info(kaisuu);

	maisuu = qtyOfCards(cards);

	for (i = 0; i < 5; i++)
	{
		for (j = 1; j < 14; j++)
		{
			if (cards[i][j] == 1 || cards[i][j] == 2)
			{
				a = i;
				b = j;

				//if(i==0)suitsum=suitsum+1;//マークを1,2,4,8の組み合わせで表すことにした。
				//if(i==1)suitsum=suitsum+2;
				//if(i==2)suitsum=suitsum+4;
				//if(i==3)suitsum=suitsum+8;

				if (j == 6)
					eight = eight + 1;
				//if(cards[i][j]==2)state.kumi_info[kaisuu][13]=1;
			}
		}
	}

	if (maisuu == 2)
	{
		for (j = b + 1; j < 14; j++)
		{
			if (mark == 3 && state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0)
				count++;
			if (mark == 6 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count++;
			if (mark == 12 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
			if (mark == 5 && state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count++;
			if (mark == 9 && state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
			if (mark == 10 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
		}
	}

	if (maisuu == 3)
	{
		for (j = b + 1; j < 14; j++)
		{
			if (mark == 7 && state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count++;
			if (mark == 14 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
			if (mark == 11 && state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
			if (mark == 13 && state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
		}
	}

	if (maisuu == 4)
	{
		for (j = b - 1; j > 0; j--)
		{ //革命
			if (mark == 15 && state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count++;
		}
	}

	//革命用の評価値の設定

	if (maisuu == 2)
	{
		for (j = b - 1; j > 0; j--)
		{
			if (mark == 3 && state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0)
				count_rev++;
			if (mark == 6 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count_rev++;
			if (mark == 12 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
			if (mark == 5 && state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count_rev++;
			if (mark == 9 && state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
			if (mark == 10 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
		}
	}

	if (maisuu == 3)
	{
		for (j = b - 1; j > 0; j--)
		{
			if (mark == 7 && state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0)
				count_rev++;
			if (mark == 14 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
			if (mark == 11 && state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
			if (mark == 13 && state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
		}
	}

	if (maisuu == 4)
	{
		for (j = b + 1; j < 14; j++)
		{ //革命
			if (mark == 15 && state.submitted_cards_plus_hands[0][j] == 0 && state.submitted_cards_plus_hands[1][j] == 0 && state.submitted_cards_plus_hands[2][j] == 0 && state.submitted_cards_plus_hands[3][j] == 0)
				count_rev++;
		}
	}
	//ここまで

	if (eight != 0)
	{
		/*
		value=teisuu * maisuu - count+20;//8は20点加点することにした。
		if(value>(100*maisuu))value=100*maisuu;
		*/
		value = 100 * maisuu;
		new_value = 100;
		/*
		value_rev=teisuu * maisuu - count_rev+20;//8は20点加点することにした。
		if(value_rev>(100*maisuu))value_rev=100*maisuu;
		*/
		value_rev = 100 * maisuu;
		new_value_rev = 100;
	}
	else
	{
		//value=teisuu * maisuu - count-state.joker_risk+state.player_number_point*maisuu;
		//value_rev=teisuu * maisuu - count_rev-state.joker_risk+state.player_number_point*maisuu;

		value = teisuu * maisuu - count;
		value_rev = teisuu * maisuu - count_rev;

		//new_value=value/maisuu;
		//new_value_rev=value_rev/maisuu;
	}
	/*
	state.kumi_info[kaisuu][0]=2;
	state.kumi_info[kaisuu][1]=maisuu;
	state.kumi_info[kaisuu][2]=suitsum;
	state.kumi_info[kaisuu][3]=b;
	state.kumi_info[kaisuu][4]=b;
	state.kumi_info[kaisuu][5]=value;
	state.kumi_info[kaisuu][6]=value/maisuu;
	state.kumi_info[kaisuu][7]=value_rev;
	state.kumi_info[kaisuu][8]=value_rev/maisuu;
	
	if(eight!=0){
		state.kumi_info[kaisuu][9]=100*maisuu+1;
		state.kumi_info[kaisuu][10]=101;
		state.kumi_info[kaisuu][11]=100*maisuu+1;
		state.kumi_info[kaisuu][12]=101;
	}else{
		state.kumi_info[kaisuu][9]=value;//流し
		state.kumi_info[kaisuu][10]=value/maisuu;
		state.kumi_info[kaisuu][11]=value_rev;//流し
		state.kumi_info[kaisuu][12]=value_rev/maisuu;
	}
	
	//state.kumi_info[kaisuu][13]=0;//仮
	if(maisuu>=4){
	state.kumi_info[kaisuu][14]=1;
	}
	
	//outputkumi_info(kaisuu);
	
	*/
	if (state.rev == 0)
	{
		return value;
		//	return new_value;
	}
	else
	{
		return value_rev;
		//	return new_value_rev;
	}
}

int setvalue_kaidan_shibari3(int cards[8][15], int mark)
{

	int i = 0, j = 0, a = 0, b = 0, value = 0, teisuu = 100, count = 0, maisuu = 0, eight = 0;
	int c = 0, d = 0, value_rev = 0, count_rev = 0;
	int new_value = 0, new_value_rev = 0;

	//clearkumi_info(kaisuu);

	maisuu = qtyOfCards(cards);

	for (i = 0; i < 5; i++)
	{
		for (j = 1; j < 14; j++)
		{
			if (cards[i][j] == 1 || cards[i][j] == 2)
			{
				a = i;
				b = j;

				if (j == 6)
					eight = eight + 1;
				//if(cards[i][j]==2)state.kumi_info[kaisuu][13]=1;
			}
		}
	}

	if (maisuu == 3)
	{
		//for(i=0;i<4;i++){
		for (j = b + 1; j < 15 - maisuu; j++)
		{
			if (state.submitted_cards_plus_hands[mark][j] == 0 && state.submitted_cards_plus_hands[mark][j + 1] == 0 && state.submitted_cards_plus_hands[mark][j + 2] == 0)
				count++;
		}
		//}
	}

	if (maisuu == 4)
	{
		//for(i=0;i<4;i++){
		for (j = b + 1; j < 15 - maisuu; j++)
		{
			if (state.submitted_cards_plus_hands[mark][j] == 0 && state.submitted_cards_plus_hands[mark][j + 1] == 0 && state.submitted_cards_plus_hands[mark][j + 2] == 0 && state.submitted_cards_plus_hands[mark][j + 3] == 0)
				count++;
		}
		//}
	}

	if (maisuu == 5)
	{
		//for(i=0;i<4;i++){
		for (j = b + 1; j < 15 - maisuu; j++)
		{
			if (state.submitted_cards_plus_hands[mark][j] == 0 && state.submitted_cards_plus_hands[mark][j + 1] == 0 && state.submitted_cards_plus_hands[mark][j + 2] == 0 && state.submitted_cards_plus_hands[mark][j + 3] == 0 && state.submitted_cards_plus_hands[mark][j + 4] == 0)
				count++;
		}
		//}
	}

	//革命用
	for (i = 0; i < 5; i++)
	{
		for (j = 13; j > 0; j--)
		{
			if (cards[i][j] == 1 || cards[i][j] == 2)
			{
				c = i;
				d = j;
			}
		}
	}

	if (maisuu == 3)
	{
		//for(i=0;i<4;i++){
		for (j = d - 1; j > maisuu - 1; j--)
		{
			if (state.submitted_cards_plus_hands[mark][j] == 0 && state.submitted_cards_plus_hands[mark][j - 1] == 0 && state.submitted_cards_plus_hands[mark][j - 2] == 0)
				count_rev++;
		}
		//}
	}

	if (maisuu == 4)
	{
		//for(i=0;i<4;i++){
		for (j = d - 1; j > maisuu - 1; j--)
		{
			if (state.submitted_cards_plus_hands[mark][j] == 0 && state.submitted_cards_plus_hands[mark][j - 1] == 0 && state.submitted_cards_plus_hands[mark][j - 2] == 0 && state.submitted_cards_plus_hands[mark][j - 3] == 0)
				count_rev++;
		}
		//}
	}

	if (maisuu == 5)
	{
		//for(i=0;i<4;i++){
		for (j = d - 1; j > maisuu - 1; j--)
		{
			if (state.submitted_cards_plus_hands[mark][j] == 0 && state.submitted_cards_plus_hands[mark][j - 1] == 0 && state.submitted_cards_plus_hands[mark][j - 2] == 0 && state.submitted_cards_plus_hands[mark][j - 3] == 0 && state.submitted_cards_plus_hands[mark][j - 4] == 0)
				count_rev++;
		}
		//}
	}
	//ここまで

	if (eight != 0)
	{
		/*
		value=teisuu * maisuu - count+20;//8は20点加点することにした。
		if(value>(100*maisuu))value=100*maisuu;
		*/
		value = 100 * maisuu;
		new_value = 100;
		/*
		value_rev=teisuu * maisuu - count_rev+20;//8は20点加点することにした。
		if(value_rev>(100*maisuu))value_rev=100*maisuu;
		*/
		value_rev = 100 * maisuu;
		new_value_rev = 100;
	}
	else
	{
		//value=teisuu * maisuu - count-state.joker_risk+state.player_number_point*maisuu;
		//value_rev=teisuu * maisuu - count_rev-state.joker_risk+state.player_number_point*maisuu;

		value = teisuu * maisuu - count;
		value_rev = teisuu * maisuu - count_rev;

		//printf("state.player_number_point  %d\n",state.player_number_point);
		//printf("maisuu  %d\n",maisuu);

		//new_value=value/maisuu;
		//new_value_rev=value_rev/maisuu;

		//printf("new_value  %d\n",new_value);
	}

	/*
	state.kumi_info[kaisuu][0]=3;
	state.kumi_info[kaisuu][1]=maisuu;
	state.kumi_info[kaisuu][2]=a;
	state.kumi_info[kaisuu][3]=b-maisuu+1;
	state.kumi_info[kaisuu][4]=b;
	state.kumi_info[kaisuu][5]=value;
	state.kumi_info[kaisuu][6]=value/maisuu;
	state.kumi_info[kaisuu][7]=value_rev;
	state.kumi_info[kaisuu][8]=value_rev/maisuu;
	
	if(eight!=0){
		state.kumi_info[kaisuu][9]=100*maisuu+1;
		state.kumi_info[kaisuu][10]=101;
		state.kumi_info[kaisuu][11]=100*maisuu+1;
		state.kumi_info[kaisuu][12]=101;
	}else{
		state.kumi_info[kaisuu][9]=value;//流し
		state.kumi_info[kaisuu][10]=value/maisuu;
		state.kumi_info[kaisuu][11]=value_rev;//流し
		state.kumi_info[kaisuu][12]=value_rev/maisuu;
	}
	
	//state.kumi_info[kaisuu][13]=0;//仮
	if(maisuu>=5){
	state.kumi_info[kaisuu][14]=1;
	}
	
	//outputkumi_info(kaisuu);
	*/

	if (state.rev == 0)
	{
		return value;
		//	return new_value;
	}
	else
	{
		return value_rev;
		//	return new_value_rev;
	}
}

int bunkatsu_all2(int table[8][15])
{
	int handvalue = 0, handvalue2 = 0, flag = 0, h = 0;
	int value = 0, flag2 = 0;
	int nagarenai_kumi = 0;
	int nokori_kumisuu = 0;

	int flag_kako = 0;

	if (state.rev == 0)
	{
		h = 4;
		//h=17;
		//h=19;
	}
	else
	{
		h = 5;
		//h=18;
		//h=20;
	}

	//printf("state.pre_rank          %d\n",state.pre_rank);

	if (state.cause_rev == 1)
	{
		/*
		if(state.game_count==4406){
			printf("bunkatsu\n");
		}
		*/

		//<_a>ではなく、ペアや階段の構成枚数に制限をかけない方を使用。
		if (state.rev == 0)
		{

			bunkatsu_noprint(table);
			if (value < state.kumi_info[13][h])
			{ //<=
				value = state.kumi_info[13][h];
				flag2 = 2;
				nokori_kumisuu = state.kumi_info[13][0] - state.kumi_info[13][6];
			}
			if (state.kumi_info[13][0] - state.kumi_info[13][6] <= 1)
				flag = 2;

			bunkatsu_noprint2(table);
			if (value <= state.kumi_info[13][h] && state.kumi_info[13][0] - state.kumi_info[13][6] <= nokori_kumisuu)
			{
				value = state.kumi_info[13][h];
				flag2 = 1;
				nokori_kumisuu = state.kumi_info[13][0] - state.kumi_info[13][6];
			}
			if (state.kumi_info[13][0] - state.kumi_info[13][6] < nokori_kumisuu)
			{
				value = state.kumi_info[13][h];
				flag2 = 1;
				nokori_kumisuu = state.kumi_info[13][0] - state.kumi_info[13][6];
			}
			if (state.kumi_info[13][0] - state.kumi_info[13][6] <= 1)
				flag = 1;

			//if(flag==1)bunkatsu_noprint2(table);
			if (flag == 2)
				bunkatsu_noprint(table);
			//if(flag==3)bunkatsu_noprint2_c(table);
			//if(flag==4)bunkatsu_noprint_c(table);

			if (flag == 0)
			{
				//if(flag2==1)bunkatsu_noprint2(table);
				if (flag2 == 2)
					bunkatsu_noprint(table);
				//if(flag2==3)bunkatsu_noprint2_c(table);
				//if(flag2==4)bunkatsu_noprint_c(table);
			}
		}
		else
		{

			bunkatsu_noprint_rev(table);
			if (state.kumi_info[13][0] - state.kumi_info[13][7] <= 1)
				flag = 2;
			if (value < state.kumi_info[13][h])
			{ //<=
				value = state.kumi_info[13][h];
				flag2 = 2;
				nokori_kumisuu = state.kumi_info[13][0] - state.kumi_info[13][7];
			}

			bunkatsu_noprint2_rev(table);
			if (state.kumi_info[13][0] - state.kumi_info[13][7] <= 1)
				flag = 1;
			if (value <= state.kumi_info[13][h] && state.kumi_info[13][0] - state.kumi_info[13][7] <= nokori_kumisuu)
			{
				value = state.kumi_info[13][h];
				flag2 = 1;
				nokori_kumisuu = state.kumi_info[13][0] - state.kumi_info[13][7];
			}
			if (state.kumi_info[13][0] - state.kumi_info[13][7] < nokori_kumisuu)
			{
				value = state.kumi_info[13][h];
				flag2 = 1;
				nokori_kumisuu = state.kumi_info[13][0] - state.kumi_info[13][7];
			}

			//if(flag==1)bunkatsu_noprint2_rev(table);
			if (flag == 2)
				bunkatsu_noprint_rev(table);
			//if(flag==3)bunkatsu_noprint2_c(table);
			//if(flag==4)bunkatsu_noprint_c(table);

			if (flag == 0)
			{
				//if(flag2==1)bunkatsu_noprint2_rev(table);
				if (flag2 == 2)
					bunkatsu_noprint_rev(table);
				//if(flag2==3)bunkatsu_noprint2_c(table);
				//if(flag2==4)bunkatsu_noprint_c(table);
			}
		}
	}

	if (state.finish == 1)
	{
		/*
		if(state.game_count==4406){
			printf("bunkatsu2\n");
		}
		*/
		if (state.rev == 0)
		{

			bunkatsu_noprint_c(table);
			if (state.kumi_info[13][0] - state.kumi_info[13][6] <= 1)
				flag = 4;
			if (value < state.kumi_info[13][h])
			{
				value = state.kumi_info[13][h];
				flag2 = 4;
				nokori_kumisuu = state.kumi_info[13][0] - state.kumi_info[13][6];
			}

			bunkatsu_noprint2_c(table);
			if (state.kumi_info[13][0] - state.kumi_info[13][6] <= 1)
				flag = 3;
			if (value < state.kumi_info[13][h] && state.kumi_info[13][0] - state.kumi_info[13][6] <= nokori_kumisuu)
			{
				value = state.kumi_info[13][h];
				flag2 = 3;
				nokori_kumisuu = state.kumi_info[13][0] - state.kumi_info[13][6];
			}
			if (value < state.kumi_info[13][h] || state.kumi_info[13][0] - state.kumi_info[13][6] < nokori_kumisuu)
			{
				value = state.kumi_info[13][h];
				flag2 = 3;
				nokori_kumisuu = state.kumi_info[13][0] - state.kumi_info[13][6];
			}

			//if(flag==3)bunkatsu_noprint2_a_c(table);
			if (flag == 4)
				bunkatsu_noprint_c(table);

			if (flag == 0)
			{
				//if(flag2==3)bunkatsu_noprint2_a_c(table);
				if (flag2 == 4)
					bunkatsu_noprint_c(table);
			}
		}
		else
		{

			bunkatsu_noprint_c(table);
			if (state.kumi_info[13][0] - state.kumi_info[13][7] <= 1)
				flag = 4;
			if (value < state.kumi_info[13][h])
			{
				value = state.kumi_info[13][h];
				flag2 = 4;
				nokori_kumisuu = state.kumi_info[13][0] - state.kumi_info[13][7];
			}

			bunkatsu_noprint2_c(table);
			if (state.kumi_info[13][0] - state.kumi_info[13][7] <= 1)
				flag = 3;
			if (value < state.kumi_info[13][h] && state.kumi_info[13][0] - state.kumi_info[13][7] <= nokori_kumisuu)
			{
				value = state.kumi_info[13][h];
				flag2 = 3;
				nokori_kumisuu = state.kumi_info[13][0] - state.kumi_info[13][7];
			}
			if (state.kumi_info[13][0] - state.kumi_info[13][7] < nokori_kumisuu)
			{
				value = state.kumi_info[13][h];
				flag2 = 3;
				nokori_kumisuu = state.kumi_info[13][0] - state.kumi_info[13][7];
			}

			//if(flag==3)bunkatsu_noprint2_a_c(table);
			if (flag == 4)
				bunkatsu_noprint_c(table);

			if (flag == 0)
			{
				//if(flag2==3)bunkatsu_noprint2_a_c(table);
				if (flag2 == 4)
					bunkatsu_noprint_c(table);
			}
		}
	}

	if (state.cause_rev == 0 && state.finish == 0)
	{
		/*
		if(state.game_count==4406){
			printf("bunkatsu3\n");
			outputCards(table);
		}
		*/

		if (state.rev == 0)
		{

			if (flag_kako == 0)
			{

				bunkatsu_noprint_a(table);
				if (state.kumi_info[13][0] - state.kumi_info[13][6] <= 1)
				{
					flag = 2;
					//state.flag_b1=2;
				}
				if (value <= state.kumi_info[13][h])
				{ //<=
					value = state.kumi_info[13][h];
					flag2 = 2;
					//nagarenai_kumi=state.kumi_info[13][0]-state.kumi_info[13][6];
					//state.flag_b1=2;
					nokori_kumisuu = state.kumi_info[13][0] - state.kumi_info[13][6];
				}

				bunkatsu_noprint2_a(table);
				//bunkatsu_noprint2_a_c(table);
				if (state.kumi_info[13][0] - state.kumi_info[13][6] <= 1)
				{
					flag = 1;
					//state.flag_b1=1;
				}
				if (value <= state.kumi_info[13][h] && state.kumi_info[13][0] - state.kumi_info[13][6] <= nokori_kumisuu)
				{ //||state.kumi_info[13][0]-state.kumi_info[13][6]<nagarenai_kumi
					value = state.kumi_info[13][h];
					flag2 = 1;
					//state.flag_b1=1;
					nokori_kumisuu = state.kumi_info[13][0] - state.kumi_info[13][6];
				}
				if (state.kumi_info[13][0] - state.kumi_info[13][6] < nokori_kumisuu)
				{ //||state.kumi_info[13][0]-state.kumi_info[13][6]<nagarenai_kumi
					value = state.kumi_info[13][h];
					flag2 = 1;
					//state.flag_b1=1;
					nokori_kumisuu = state.kumi_info[13][0] - state.kumi_info[13][6];
				}

				//if(flag==1)bunkatsu_noprint2_a(table);
				if (flag == 2)
					bunkatsu_noprint_a(table);
				//if(flag==3)bunkatsu_noprint2_a_c(table);
				//if(flag==4)bunkatsu_noprint_a_c(table);

				if (flag == 0)
				{
					//if(flag2==1)bunkatsu_noprint2_a(table);
					if (flag2 == 2)
						bunkatsu_noprint_a(table);
					//if(flag2==3)bunkatsu_noprint2_a_c(table);
					//if(flag2==4)bunkatsu_noprint_a_c(table);
				}
			}
		}
		else
		{

			bunkatsu_noprint_a_rev(table);
			handvalue2 = state.kumi_info[13][h];
			if (state.kumi_info[13][0] - state.kumi_info[13][7] <= 1)
			{
				flag = 2;
			}
			if (value <= state.kumi_info[13][h])
			{ //<=
				value = state.kumi_info[13][h];
				flag2 = 2;
				nokori_kumisuu = state.kumi_info[13][0] - state.kumi_info[13][7];
			}

			bunkatsu_noprint2_a_rev(table);
			//bunkatsu_noprint2_a_c(table);

			handvalue = state.kumi_info[13][h];
			if (state.kumi_info[13][0] - state.kumi_info[13][7] <= 1)
			{
				flag = 1;
			}
			if (value <= state.kumi_info[13][h] && state.kumi_info[13][0] - state.kumi_info[13][7] <= nokori_kumisuu)
			{ //||state.kumi_info[13][0]-state.kumi_info[13][7]<nagarenai_kumi
				value = state.kumi_info[13][h];
				flag2 = 1;
				nokori_kumisuu = state.kumi_info[13][0] - state.kumi_info[13][7];
			}
			if (state.kumi_info[13][0] - state.kumi_info[13][7] < nokori_kumisuu)
			{ //||state.kumi_info[13][0]-state.kumi_info[13][7]<nagarenai_kumi
				value = state.kumi_info[13][h];
				flag2 = 1;
				nokori_kumisuu = state.kumi_info[13][0] - state.kumi_info[13][7];
			}

			//if(flag==1)bunkatsu_noprint2_a_rev(table);
			if (flag == 2)
				bunkatsu_noprint_a_rev(table);
			//if(flag==3)bunkatsu_noprint2_a_c(table);
			//if(flag==4)bunkatsu_noprint_a_c(table);

			if (flag == 0)
			{
				//if(flag2==1)bunkatsu_noprint2_a_rev(table);
				if (flag2 == 2)
					bunkatsu_noprint_a_rev(table);
				//if(flag2==3)bunkatsu_noprint2_a_c(table);
				//if(flag2==4)bunkatsu_noprint_a_c(table);
			}
		}
		/*
			if(state.game_count==4406){
				printf("bunkatsu3\n");
				outputCards(table);
			}
		*/
	}
}

int bunkatsu_noprint(int table[8][15])
{
	int i, j, k, count = 0, point = 0, totalpoint = 0;
	int kaidantable[8][15] = {{0}};
	int pairtable[8][15] = {{0}};
	int copytable[8][15] = {{0}};
	int kaidanlen = 0;
	int pairmai = 0;
	int maisuu = 0;
	int c = 0, kumisuu = 0;
	int count2 = 0;

	//階段→ペア→単体

	clearTable(state.kumi0); //追加.組を保存する配列を一試合ごとにリセット。
	clearTable(state.kumi1);
	clearTable(state.kumi2);
	clearTable(state.kumi3);
	clearTable(state.kumi4);
	clearTable(state.kumi5);
	clearTable(state.kumi6);
	clearTable(state.kumi7);
	clearTable(state.kumi8);
	clearTable(state.kumi9);
	clearTable(state.kumi10);
	clearTable(state.kumi11);
	clearTable(state.kumi12);
	/*
						for(i=0;i<14;i++){
							clearkumi_info(i);
						}
						*/
	clearkumi_info2();

	for (i = 0; i < 5; i++)
	{
		for (j = 0; j < 15; j++)
		{
			copytable[i][j] = table[i][j]; //copytableの作成
		}
	}

	while (qtyOfCards(copytable) > 0)
	{

		maisuu = 0; //リセット

		for (i = 0; i < 4; i++)
		{ //階段tableの作成
			for (j = 13 - 1; j > 0; j--)
			{
				if (copytable[i][j] == 1 || copytable[i][j] == 2)
				{
					kaidantable[i][j] = kaidantable[i][j + 1] + 1;
				}
				else
				{
					kaidantable[i][j] = 0;
				}
			}
		}

		for (i = 0; i < 4; i++)
		{ //pairtableの作成
			for (j = 1; j < 14 - 1; j++)
			{
				if (copytable[i][j] == 1 || copytable[i][j] == 2)
				{
					pairtable[i][j] = copytable[0][j] + copytable[1][j] + copytable[2][j] + copytable[3][j];
					if (copytable[0][j] == 2 || copytable[1][j] == 2 || copytable[2][j] == 2 || copytable[3][j] == 2)
					{
						pairtable[i][j] = pairtable[i][j] - 1;
					}
				}
				else
				{
					pairtable[i][j] = 0;
				}
			}
		}
		/*
						for(i=0;i<4;i++){//test
							for(j=1;j<14;j++){
								if(kaidantable[i][j]>=5)kaidantable[i][j]=4;
							}
						}
		*/
		/*
						for(i=0;i<4;i++){//test
							for(j=1;j<14;j++){
								if(pairtable[i][j]==4)pairtable[i][j]=2;
							}
						}
		*/

		if (qtyOfCards3(kaidantable) > 0)
		{

			for (kaidanlen = 11; kaidanlen > 2; kaidanlen--)
			{
				if (maisuu > 2)
					break;
				for (i = 0; i < 4; i++)
				{
					if (maisuu > 2)
						break;
					for (j = 1; j < 14; j++)
					{
						if (maisuu > 2)
							break;
						if (kaidantable[i][j] == kaidanlen)
						{
							for (k = 0; k < kaidanlen; k++)
							{

								if (count == 0)
								{
									state.kumi0[i][j + k] = copytable[i][j + k];
								}
								else if (count == 1)
								{
									state.kumi1[i][j + k] = copytable[i][j + k];
								}
								else if (count == 2)
								{
									state.kumi2[i][j + k] = copytable[i][j + k];
								}
								else if (count == 3)
								{
									state.kumi3[i][j + k] = copytable[i][j + k];
								}
								else if (count == 4)
								{
									state.kumi4[i][j + k] = copytable[i][j + k];
								}
								else if (count == 5)
								{
									state.kumi5[i][j + k] = copytable[i][j + k];
								}
								else if (count == 6)
								{
									state.kumi6[i][j + k] = copytable[i][j + k];
								}
								else if (count == 7)
								{
									state.kumi7[i][j + k] = copytable[i][j + k];
								}
								else if (count == 8)
								{
									state.kumi8[i][j + k] = copytable[i][j + k];
								}
								else if (count == 9)
								{
									state.kumi9[i][j + k] = copytable[i][j + k];
								}
								else if (count == 10)
								{
									state.kumi10[i][j + k] = copytable[i][j + k];
								}
								else if (count == 11)
								{
									state.kumi11[i][j + k] = copytable[i][j + k];
								}
								else if (count == 12)
								{
									state.kumi12[i][j + k] = copytable[i][j + k];
								}
								copytable[i][j + k] = 0;
							}

							if (count == 0)
							{
								maisuu = qtyOfCards(state.kumi0);
							}
							else if (count == 1)
							{
								maisuu = qtyOfCards(state.kumi1);
							}
							else if (count == 2)
							{
								maisuu = qtyOfCards(state.kumi2);
							}
							else if (count == 3)
							{
								maisuu = qtyOfCards(state.kumi3);
							}
							else if (count == 4)
							{
								maisuu = qtyOfCards(state.kumi4);
							}
							else if (count == 5)
							{
								maisuu = qtyOfCards(state.kumi5);
							}
							else if (count == 6)
							{
								maisuu = qtyOfCards(state.kumi6);
							}
							else if (count == 7)
							{
								maisuu = qtyOfCards(state.kumi7);
							}
							else if (count == 8)
							{
								maisuu = qtyOfCards(state.kumi8);
							}
							else if (count == 9)
							{
								maisuu = qtyOfCards(state.kumi9);
							}
							else if (count == 10)
							{
								maisuu = qtyOfCards(state.kumi10);
							}
							else if (count == 11)
							{
								maisuu = qtyOfCards(state.kumi11);
							}
							else if (count == 12)
							{
								maisuu = qtyOfCards(state.kumi12);
							}
						}
					}
				}
			}
			if (count == 0)
			{
				point = setvalue_kaidan(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_kaidan(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_kaidan(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_kaidan(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_kaidan(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_kaidan(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_kaidan(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_kaidan(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_kaidan(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_kaidan(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_kaidan(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_kaidan(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_kaidan(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		else if (qtyOfCards2(pairtable) > 0)
		{

			//maisuu=0;

			for (pairmai = 5; pairmai > 1; pairmai--)
			{
				if (maisuu > 1)
					break;
				for (i = 0; i < 4; i++)
				{
					if (maisuu > 1)
						break;
					for (j = 1; j < 14; j++)
					{
						if (maisuu > 1)
							break;
						if (pairtable[i][j] == pairmai)
						{

							for (k = 0; k < 4; k++)
							{
								if (copytable[k][j] == 1 || copytable[k][j] == 2)
								{

									if (count == 0)
									{
										state.kumi0[k][j] = copytable[k][j];
									}
									else if (count == 1)
									{
										state.kumi1[k][j] = copytable[k][j];
									}
									else if (count == 2)
									{
										state.kumi2[k][j] = copytable[k][j];
									}
									else if (count == 3)
									{
										state.kumi3[k][j] = copytable[k][j];
									}
									else if (count == 4)
									{
										;
										state.kumi4[k][j] = copytable[k][j];
									}
									else if (count == 5)
									{
										state.kumi5[k][j] = copytable[k][j];
									}
									else if (count == 6)
									{
										state.kumi6[k][j] = copytable[k][j];
									}
									else if (count == 7)
									{
										state.kumi7[k][j] = copytable[k][j];
									}
									else if (count == 8)
									{
										state.kumi8[k][j] = copytable[k][j];
									}
									else if (count == 9)
									{
										state.kumi9[k][j] = copytable[k][j];
									}
									else if (count == 10)
									{
										state.kumi10[k][j] = copytable[k][j];
									}
									else if (count == 11)
									{
										state.kumi11[k][j] = copytable[k][j];
									}
									else if (count == 12)
									{
										state.kumi12[k][j] = copytable[k][j];
									}

									copytable[k][j] = 0;

									count2 = count2 + 1;

									if (count2 == pairmai)
									{
										count2 = 0;
										break;
									}
								}
							}

							if (count == 0)
							{
								maisuu = qtyOfCards(state.kumi0);
							}
							else if (count == 1)
							{
								maisuu = qtyOfCards(state.kumi1);
							}
							else if (count == 2)
							{
								maisuu = qtyOfCards(state.kumi2);
							}
							else if (count == 3)
							{
								maisuu = qtyOfCards(state.kumi3);
							}
							else if (count == 4)
							{
								maisuu = qtyOfCards(state.kumi4);
							}
							else if (count == 5)
							{
								maisuu = qtyOfCards(state.kumi5);
							}
							else if (count == 6)
							{
								maisuu = qtyOfCards(state.kumi6);
							}
							else if (count == 7)
							{
								maisuu = qtyOfCards(state.kumi7);
							}
							else if (count == 8)
							{
								maisuu = qtyOfCards(state.kumi8);
							}
							else if (count == 9)
							{
								maisuu = qtyOfCards(state.kumi9);
							}
							else if (count == 10)
							{
								maisuu = qtyOfCards(state.kumi10);
							}
							else if (count == 11)
							{
								maisuu = qtyOfCards(state.kumi11);
							}
							else if (count == 12)
							{
								maisuu = qtyOfCards(state.kumi12);
							}
						}
					}
				}
			}
			if (count == 0)
			{
				point = setvalue_pair(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_pair(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_pair(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_pair(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_pair(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_pair(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_pair(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_pair(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_pair(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_pair(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_pair(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_pair(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_pair(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		else if (qtyOfCards(copytable) > 0)
		{

			for (i = 0; i < 4; i++)
			{
				if (maisuu == 1)
					break;
				for (j = 1; j < 14; j++)
				{
					if (maisuu == 1)
						break;
					if (copytable[i][j] == 1 || copytable[i][j] == 2)
					{

						if (count == 0)
						{
							state.kumi0[i][j] = copytable[i][j];
						}
						else if (count == 1)
						{
							state.kumi1[i][j] = copytable[i][j];
						}
						else if (count == 2)
						{
							state.kumi2[i][j] = copytable[i][j];
						}
						else if (count == 3)
						{
							state.kumi3[i][j] = copytable[i][j];
						}
						else if (count == 4)
						{
							state.kumi4[i][j] = copytable[i][j];
						}
						else if (count == 5)
						{
							state.kumi5[i][j] = copytable[i][j];
						}
						else if (count == 6)
						{
							state.kumi6[i][j] = copytable[i][j];
						}
						else if (count == 7)
						{
							state.kumi7[i][j] = copytable[i][j];
						}
						else if (count == 8)
						{
							state.kumi8[i][j] = copytable[i][j];
						}
						else if (count == 9)
						{
							state.kumi9[i][j] = copytable[i][j];
						}
						else if (count == 10)
						{
							state.kumi10[i][j] = copytable[i][j];
						}
						else if (count == 11)
						{
							state.kumi11[i][j] = copytable[i][j];
						}
						else if (count == 12)
						{
							state.kumi12[i][j] = copytable[i][j];
						}

						copytable[i][j] = 0;

						if (count == 0)
						{
							maisuu = qtyOfCards(state.kumi0);
						}
						else if (count == 1)
						{
							maisuu = qtyOfCards(state.kumi1);
						}
						else if (count == 2)
						{
							maisuu = qtyOfCards(state.kumi2);
						}
						else if (count == 3)
						{
							maisuu = qtyOfCards(state.kumi3);
						}
						else if (count == 4)
						{
							maisuu = qtyOfCards(state.kumi4);
						}
						else if (count == 5)
						{
							maisuu = qtyOfCards(state.kumi5);
						}
						else if (count == 6)
						{
							maisuu = qtyOfCards(state.kumi6);
						}
						else if (count == 7)
						{
							maisuu = qtyOfCards(state.kumi7);
						}
						else if (count == 8)
						{
							maisuu = qtyOfCards(state.kumi8);
						}
						else if (count == 9)
						{
							maisuu = qtyOfCards(state.kumi9);
						}
						else if (count == 10)
						{
							maisuu = qtyOfCards(state.kumi10);
						}
						else if (count == 11)
						{
							maisuu = qtyOfCards(state.kumi11);
						}
						else if (count == 12)
						{
							maisuu = qtyOfCards(state.kumi12);
						}
					}
				}
			}

			if (maisuu == 0 && joker_check(copytable) == 2)
			{
				if (count == 0)
				{
					state.kumi0[4][1] = 2;
				}
				else if (count == 1)
				{
					state.kumi1[4][1] = 2;
				}
				else if (count == 2)
				{
					state.kumi2[4][1] = 2;
				}
				else if (count == 3)
				{
					state.kumi3[4][1] = 2;
				}
				else if (count == 4)
				{
					state.kumi4[4][1] = 2;
				}
				else if (count == 5)
				{
					state.kumi5[4][1] = 2;
				}
				else if (count == 6)
				{
					state.kumi6[4][1] = 2;
				}
				else if (count == 7)
				{
					state.kumi7[4][1] = 2;
				}
				else if (count == 8)
				{
					state.kumi8[4][1] = 2;
				}
				else if (count == 9)
				{
					state.kumi9[4][1] = 2;
				}
				else if (count == 10)
				{
					state.kumi10[4][1] = 2;
				}
				else if (count == 11)
				{
					state.kumi11[4][1] = 2;
				}
				else if (count == 12)
				{
					state.kumi12[4][1] = 2;
				}
				copytable[4][1] = 0;
			}

			if (count == 0)
			{
				point = setvalue_single(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_single(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_single(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_single(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_single(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_single(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_single(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_single(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_single(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_single(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_single(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_single(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_single(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		/*printf("kumi:%d   point:%d\n",count,point);
		
						for(i=0;i<5;i++){
    	 					for(j=0;j<15;j++){
      						
    	 						if(count==0){
    	 							printf("%i ",state.kumi0[i][j]);
								}else if(count==1){
									printf("%i ",state.kumi1[i][j]);
								}else if(count==2){
									printf("%i ",state.kumi2[i][j]);
								}else if(count==3){
									printf("%i ",state.kumi3[i][j]);
								}else if(count==4){
									printf("%i ",state.kumi4[i][j]);
								}else if(count==5){
									printf("%i ",state.kumi5[i][j]);
								}else if(count==6){
									printf("%i ",state.kumi6[i][j]);
								}else if(count==7){
									printf("%i ",state.kumi7[i][j]);
								}else if(count==8){
									printf("%i ",state.kumi8[i][j]);
								}else if(count==9){
									printf("%i ",state.kumi9[i][j]);
								}else if(count==10){
									printf("%i ",state.kumi10[i][j]);
								}
    						}
    						printf("\n");

						}

						printf("\n");*/

		count++;
	}
	setvalue_hands();
}

int bunkatsu_noprint2(int table[8][15])
{
	int i, j, k, count = 0, point = 0, totalpoint = 0;
	int kaidantable[8][15] = {{0}};
	int pairtable[8][15] = {{0}};
	int copytable[8][15] = {{0}};
	int kaidanlen = 0;
	int pairmai = 0;
	int maisuu = 0;
	int c = 0, kumisuu = 0;
	int count2 = 0;

	//ペア→階段→単体

	clearTable(state.kumi0); //追加.組を保存する配列を一試合ごとにリセット。
	clearTable(state.kumi1);
	clearTable(state.kumi2);
	clearTable(state.kumi3);
	clearTable(state.kumi4);
	clearTable(state.kumi5);
	clearTable(state.kumi6);
	clearTable(state.kumi7);
	clearTable(state.kumi8);
	clearTable(state.kumi9);
	clearTable(state.kumi10);
	clearTable(state.kumi11);
	clearTable(state.kumi12);
	/*
						for(i=0;i<14;i++){
							clearkumi_info(i);
						}
						*/
	clearkumi_info2();
	for (i = 0; i < 5; i++)
	{
		for (j = 0; j < 15; j++)
		{
			copytable[i][j] = table[i][j]; //copytableの作成
		}
	}

	while (qtyOfCards(copytable) > 0)
	{

		maisuu = 0; //リセット

		for (i = 0; i < 4; i++)
		{ //階段tableの作成
			for (j = 13 - 1; j > 0; j--)
			{
				if (copytable[i][j] == 1 || copytable[i][j] == 2)
				{
					kaidantable[i][j] = kaidantable[i][j + 1] + 1;
				}
				else
				{
					kaidantable[i][j] = 0;
				}
			}
		}

		for (i = 0; i < 4; i++)
		{ //pairtableの作成
			for (j = 1; j < 14 - 1; j++)
			{
				if (copytable[i][j] == 1 || copytable[i][j] == 2)
				{
					pairtable[i][j] = copytable[0][j] + copytable[1][j] + copytable[2][j] + copytable[3][j];
					if (copytable[0][j] == 2 || copytable[1][j] == 2 || copytable[2][j] == 2 || copytable[3][j] == 2)
					{
						pairtable[i][j] = pairtable[i][j] - 1;
					}
				}
				else
				{
					pairtable[i][j] = 0;
				}
			}
		}
		/*
						for(i=0;i<4;i++){//test
							for(j=1;j<14;j++){
								if(kaidantable[i][j]>=5)kaidantable[i][j]=4;
							}
						}
		*/
		/*
						for(i=0;i<4;i++){//test
							for(j=1;j<14;j++){
								if(pairtable[i][j]==4)pairtable[i][j]=2;
							}
						}
		*/

		if (qtyOfCards2(pairtable) > 0)
		{

			//maisuu=0;

			for (pairmai = 5; pairmai > 1; pairmai--)
			{
				if (maisuu > 1)
					break;
				for (i = 0; i < 4; i++)
				{
					if (maisuu > 1)
						break;
					for (j = 1; j < 14; j++)
					{
						if (maisuu > 1)
							break;
						if (pairtable[i][j] == pairmai)
						{

							for (k = 0; k < 4; k++)
							{
								if (copytable[k][j] == 1 || copytable[k][j] == 2)
								{

									if (count == 0)
									{
										state.kumi0[k][j] = copytable[k][j];
									}
									else if (count == 1)
									{
										state.kumi1[k][j] = copytable[k][j];
									}
									else if (count == 2)
									{
										state.kumi2[k][j] = copytable[k][j];
									}
									else if (count == 3)
									{
										state.kumi3[k][j] = copytable[k][j];
									}
									else if (count == 4)
									{
										;
										state.kumi4[k][j] = copytable[k][j];
									}
									else if (count == 5)
									{
										state.kumi5[k][j] = copytable[k][j];
									}
									else if (count == 6)
									{
										state.kumi6[k][j] = copytable[k][j];
									}
									else if (count == 7)
									{
										state.kumi7[k][j] = copytable[k][j];
									}
									else if (count == 8)
									{
										state.kumi8[k][j] = copytable[k][j];
									}
									else if (count == 9)
									{
										state.kumi9[k][j] = copytable[k][j];
									}
									else if (count == 10)
									{
										state.kumi10[k][j] = copytable[k][j];
									}
									else if (count == 11)
									{
										state.kumi11[k][j] = copytable[k][j];
									}
									else if (count == 12)
									{
										state.kumi12[k][j] = copytable[k][j];
									}

									copytable[k][j] = 0;

									count2 = count2 + 1;

									if (count2 == pairmai)
									{
										count2 = 0;
										break;
									}
								}
							}

							if (count == 0)
							{
								maisuu = qtyOfCards(state.kumi0);
							}
							else if (count == 1)
							{
								maisuu = qtyOfCards(state.kumi1);
							}
							else if (count == 2)
							{
								maisuu = qtyOfCards(state.kumi2);
							}
							else if (count == 3)
							{
								maisuu = qtyOfCards(state.kumi3);
							}
							else if (count == 4)
							{
								maisuu = qtyOfCards(state.kumi4);
							}
							else if (count == 5)
							{
								maisuu = qtyOfCards(state.kumi5);
							}
							else if (count == 6)
							{
								maisuu = qtyOfCards(state.kumi6);
							}
							else if (count == 7)
							{
								maisuu = qtyOfCards(state.kumi7);
							}
							else if (count == 8)
							{
								maisuu = qtyOfCards(state.kumi8);
							}
							else if (count == 9)
							{
								maisuu = qtyOfCards(state.kumi9);
							}
							else if (count == 10)
							{
								maisuu = qtyOfCards(state.kumi10);
							}
							else if (count == 11)
							{
								maisuu = qtyOfCards(state.kumi11);
							}
							else if (count == 12)
							{
								maisuu = qtyOfCards(state.kumi12);
							}
						}
					}
				}
			}
			if (count == 0)
			{
				point = setvalue_pair(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_pair(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_pair(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_pair(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_pair(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_pair(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_pair(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_pair(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_pair(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_pair(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_pair(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_pair(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_pair(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		else if (qtyOfCards3(kaidantable) > 0)
		{

			for (kaidanlen = 11; kaidanlen > 2; kaidanlen--)
			{
				if (maisuu > 2)
					break;
				for (i = 0; i < 4; i++)
				{
					if (maisuu > 2)
						break;
					for (j = 1; j < 14; j++)
					{
						if (maisuu > 2)
							break;
						if (kaidantable[i][j] == kaidanlen)
						{
							for (k = 0; k < kaidanlen; k++)
							{

								if (count == 0)
								{
									state.kumi0[i][j + k] = copytable[i][j + k];
								}
								else if (count == 1)
								{
									state.kumi1[i][j + k] = copytable[i][j + k];
								}
								else if (count == 2)
								{
									state.kumi2[i][j + k] = copytable[i][j + k];
								}
								else if (count == 3)
								{
									state.kumi3[i][j + k] = copytable[i][j + k];
								}
								else if (count == 4)
								{
									state.kumi4[i][j + k] = copytable[i][j + k];
								}
								else if (count == 5)
								{
									state.kumi5[i][j + k] = copytable[i][j + k];
								}
								else if (count == 6)
								{
									state.kumi6[i][j + k] = copytable[i][j + k];
								}
								else if (count == 7)
								{
									state.kumi7[i][j + k] = copytable[i][j + k];
								}
								else if (count == 8)
								{
									state.kumi8[i][j + k] = copytable[i][j + k];
								}
								else if (count == 9)
								{
									state.kumi9[i][j + k] = copytable[i][j + k];
								}
								else if (count == 10)
								{
									state.kumi10[i][j + k] = copytable[i][j + k];
								}
								else if (count == 11)
								{
									state.kumi11[i][j + k] = copytable[i][j + k];
								}
								else if (count == 12)
								{
									state.kumi12[i][j + k] = copytable[i][j + k];
								}
								copytable[i][j + k] = 0;
							}

							if (count == 0)
							{
								maisuu = qtyOfCards(state.kumi0);
							}
							else if (count == 1)
							{
								maisuu = qtyOfCards(state.kumi1);
							}
							else if (count == 2)
							{
								maisuu = qtyOfCards(state.kumi2);
							}
							else if (count == 3)
							{
								maisuu = qtyOfCards(state.kumi3);
							}
							else if (count == 4)
							{
								maisuu = qtyOfCards(state.kumi4);
							}
							else if (count == 5)
							{
								maisuu = qtyOfCards(state.kumi5);
							}
							else if (count == 6)
							{
								maisuu = qtyOfCards(state.kumi6);
							}
							else if (count == 7)
							{
								maisuu = qtyOfCards(state.kumi7);
							}
							else if (count == 8)
							{
								maisuu = qtyOfCards(state.kumi8);
							}
							else if (count == 9)
							{
								maisuu = qtyOfCards(state.kumi9);
							}
							else if (count == 10)
							{
								maisuu = qtyOfCards(state.kumi10);
							}
							else if (count == 11)
							{
								maisuu = qtyOfCards(state.kumi11);
							}
							else if (count == 12)
							{
								maisuu = qtyOfCards(state.kumi12);
							}
						}
					}
				}
			}
			if (count == 0)
			{
				point = setvalue_kaidan(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_kaidan(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_kaidan(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_kaidan(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_kaidan(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_kaidan(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_kaidan(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_kaidan(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_kaidan(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_kaidan(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_kaidan(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_kaidan(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_kaidan(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		else if (qtyOfCards(copytable) > 0)
		{

			for (i = 0; i < 4; i++)
			{
				if (maisuu == 1)
					break;
				for (j = 1; j < 14; j++)
				{
					if (maisuu == 1)
						break;
					if (copytable[i][j] == 1 || copytable[i][j] == 2)
					{

						if (count == 0)
						{
							state.kumi0[i][j] = copytable[i][j];
						}
						else if (count == 1)
						{
							state.kumi1[i][j] = copytable[i][j];
						}
						else if (count == 2)
						{
							state.kumi2[i][j] = copytable[i][j];
						}
						else if (count == 3)
						{
							state.kumi3[i][j] = copytable[i][j];
						}
						else if (count == 4)
						{
							state.kumi4[i][j] = copytable[i][j];
						}
						else if (count == 5)
						{
							state.kumi5[i][j] = copytable[i][j];
						}
						else if (count == 6)
						{
							state.kumi6[i][j] = copytable[i][j];
						}
						else if (count == 7)
						{
							state.kumi7[i][j] = copytable[i][j];
						}
						else if (count == 8)
						{
							state.kumi8[i][j] = copytable[i][j];
						}
						else if (count == 9)
						{
							state.kumi9[i][j] = copytable[i][j];
						}
						else if (count == 10)
						{
							state.kumi10[i][j] = copytable[i][j];
						}
						else if (count == 11)
						{
							state.kumi11[i][j] = copytable[i][j];
						}
						else if (count == 12)
						{
							state.kumi12[i][j] = copytable[i][j];
						}

						copytable[i][j] = 0;

						if (count == 0)
						{
							maisuu = qtyOfCards(state.kumi0);
						}
						else if (count == 1)
						{
							maisuu = qtyOfCards(state.kumi1);
						}
						else if (count == 2)
						{
							maisuu = qtyOfCards(state.kumi2);
						}
						else if (count == 3)
						{
							maisuu = qtyOfCards(state.kumi3);
						}
						else if (count == 4)
						{
							maisuu = qtyOfCards(state.kumi4);
						}
						else if (count == 5)
						{
							maisuu = qtyOfCards(state.kumi5);
						}
						else if (count == 6)
						{
							maisuu = qtyOfCards(state.kumi6);
						}
						else if (count == 7)
						{
							maisuu = qtyOfCards(state.kumi7);
						}
						else if (count == 8)
						{
							maisuu = qtyOfCards(state.kumi8);
						}
						else if (count == 9)
						{
							maisuu = qtyOfCards(state.kumi9);
						}
						else if (count == 10)
						{
							maisuu = qtyOfCards(state.kumi10);
						}
						else if (count == 11)
						{
							maisuu = qtyOfCards(state.kumi11);
						}
						else if (count == 12)
						{
							maisuu = qtyOfCards(state.kumi12);
						}
					}
				}
			}

			if (maisuu == 0 && joker_check(copytable) == 2)
			{
				if (count == 0)
				{
					state.kumi0[4][1] = 2;
				}
				else if (count == 1)
				{
					state.kumi1[4][1] = 2;
				}
				else if (count == 2)
				{
					state.kumi2[4][1] = 2;
				}
				else if (count == 3)
				{
					state.kumi3[4][1] = 2;
				}
				else if (count == 4)
				{
					state.kumi4[4][1] = 2;
				}
				else if (count == 5)
				{
					state.kumi5[4][1] = 2;
				}
				else if (count == 6)
				{
					state.kumi6[4][1] = 2;
				}
				else if (count == 7)
				{
					state.kumi7[4][1] = 2;
				}
				else if (count == 8)
				{
					state.kumi8[4][1] = 2;
				}
				else if (count == 9)
				{
					state.kumi9[4][1] = 2;
				}
				else if (count == 10)
				{
					state.kumi10[4][1] = 2;
				}
				else if (count == 11)
				{
					state.kumi11[4][1] = 2;
				}
				else if (count == 12)
				{
					state.kumi12[4][1] = 2;
				}
				copytable[4][1] = 0;
			}

			if (count == 0)
			{
				point = setvalue_single(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_single(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_single(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_single(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_single(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_single(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_single(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_single(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_single(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_single(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_single(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_single(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_single(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		/*printf("kumi:%d   point:%d\n",count,point);
		
						for(i=0;i<5;i++){
    	 					for(j=0;j<15;j++){
      						
    	 						if(count==0){
    	 							printf("%i ",state.kumi0[i][j]);
								}else if(count==1){
									printf("%i ",state.kumi1[i][j]);
								}else if(count==2){
									printf("%i ",state.kumi2[i][j]);
								}else if(count==3){
									printf("%i ",state.kumi3[i][j]);
								}else if(count==4){
									printf("%i ",state.kumi4[i][j]);
								}else if(count==5){
									printf("%i ",state.kumi5[i][j]);
								}else if(count==6){
									printf("%i ",state.kumi6[i][j]);
								}else if(count==7){
									printf("%i ",state.kumi7[i][j]);
								}else if(count==8){
									printf("%i ",state.kumi8[i][j]);
								}else if(count==9){
									printf("%i ",state.kumi9[i][j]);
								}else if(count==10){
									printf("%i ",state.kumi10[i][j]);
								}
    						}
    						printf("\n");

						}

						printf("\n");*/

		count++;
	}
	setvalue_hands();
}

int bunkatsu_noprint_a(int table[8][15])
{
	int i, j, k, count = 0, point = 0, totalpoint = 0;
	int kaidantable[8][15] = {{0}};
	int pairtable[8][15] = {{0}};
	int copytable[8][15] = {{0}};
	int kaidanlen = 0;
	int pairmai = 0;
	int maisuu = 0;
	int c = 0, kumisuu = 0;
	int count2 = 0;

	//階段→ペア→単体

	clearTable(state.kumi0); //追加.組を保存する配列を一試合ごとにリセット。
	clearTable(state.kumi1);
	clearTable(state.kumi2);
	clearTable(state.kumi3);
	clearTable(state.kumi4);
	clearTable(state.kumi5);
	clearTable(state.kumi6);
	clearTable(state.kumi7);
	clearTable(state.kumi8);
	clearTable(state.kumi9);
	clearTable(state.kumi10);
	clearTable(state.kumi11);
	clearTable(state.kumi12);
	/*
						for(i=0;i<14;i++){
							clearkumi_info(i);
						}
						*/
	clearkumi_info2();
	for (i = 0; i < 5; i++)
	{
		for (j = 0; j < 15; j++)
		{
			copytable[i][j] = table[i][j]; //copytableの作成
		}
	}

	while (qtyOfCards(copytable) > 0)
	{

		maisuu = 0; //リセット

		for (i = 0; i < 4; i++)
		{ //階段tableの作成
			for (j = 13 - 1; j > 0; j--)
			{
				if (copytable[i][j] == 1 || copytable[i][j] == 2)
				{
					kaidantable[i][j] = kaidantable[i][j + 1] + 1;
				}
				else
				{
					kaidantable[i][j] = 0;
				}
			}
		}

		for (i = 0; i < 4; i++)
		{ //pairtableの作成
			for (j = 1; j < 14 - 1; j++)
			{
				if (copytable[i][j] == 1 || copytable[i][j] == 2)
				{
					pairtable[i][j] = copytable[0][j] + copytable[1][j] + copytable[2][j] + copytable[3][j];
					if (copytable[0][j] == 2 || copytable[1][j] == 2 || copytable[2][j] == 2 || copytable[3][j] == 2)
					{
						pairtable[i][j] = pairtable[i][j] - 1;
					}
				}
				else
				{
					pairtable[i][j] = 0;
				}
			}
		}

		for (i = 0; i < 4; i++)
		{ //test
			for (j = 1; j < 14; j++)
			{
				if (kaidantable[i][j] >= 5)
					kaidantable[i][j] = 4;
			}
		}

		for (i = 0; i < 4; i++)
		{ //test
			for (j = 1; j < 14; j++)
			{
				if (pairtable[i][j] == 4)
					pairtable[i][j] = 3;
			}
		}

		if (qtyOfCards3(kaidantable) > 0)
		{

			for (kaidanlen = 11; kaidanlen > 2; kaidanlen--)
			{
				if (maisuu > 2)
					break;
				for (i = 0; i < 4; i++)
				{
					if (maisuu > 2)
						break;
					for (j = 1; j < 14; j++)
					{
						if (maisuu > 2)
							break;
						if (kaidantable[i][j] == kaidanlen)
						{
							for (k = 0; k < kaidanlen; k++)
							{

								if (count == 0)
								{
									state.kumi0[i][j + k] = copytable[i][j + k];
								}
								else if (count == 1)
								{
									state.kumi1[i][j + k] = copytable[i][j + k];
								}
								else if (count == 2)
								{
									state.kumi2[i][j + k] = copytable[i][j + k];
								}
								else if (count == 3)
								{
									state.kumi3[i][j + k] = copytable[i][j + k];
								}
								else if (count == 4)
								{
									state.kumi4[i][j + k] = copytable[i][j + k];
								}
								else if (count == 5)
								{
									state.kumi5[i][j + k] = copytable[i][j + k];
								}
								else if (count == 6)
								{
									state.kumi6[i][j + k] = copytable[i][j + k];
								}
								else if (count == 7)
								{
									state.kumi7[i][j + k] = copytable[i][j + k];
								}
								else if (count == 8)
								{
									state.kumi8[i][j + k] = copytable[i][j + k];
								}
								else if (count == 9)
								{
									state.kumi9[i][j + k] = copytable[i][j + k];
								}
								else if (count == 10)
								{
									state.kumi10[i][j + k] = copytable[i][j + k];
								}
								else if (count == 11)
								{
									state.kumi11[i][j + k] = copytable[i][j + k];
								}
								else if (count == 12)
								{
									state.kumi12[i][j + k] = copytable[i][j + k];
								}
								copytable[i][j + k] = 0;
							}

							if (count == 0)
							{
								maisuu = qtyOfCards(state.kumi0);
							}
							else if (count == 1)
							{
								maisuu = qtyOfCards(state.kumi1);
							}
							else if (count == 2)
							{
								maisuu = qtyOfCards(state.kumi2);
							}
							else if (count == 3)
							{
								maisuu = qtyOfCards(state.kumi3);
							}
							else if (count == 4)
							{
								maisuu = qtyOfCards(state.kumi4);
							}
							else if (count == 5)
							{
								maisuu = qtyOfCards(state.kumi5);
							}
							else if (count == 6)
							{
								maisuu = qtyOfCards(state.kumi6);
							}
							else if (count == 7)
							{
								maisuu = qtyOfCards(state.kumi7);
							}
							else if (count == 8)
							{
								maisuu = qtyOfCards(state.kumi8);
							}
							else if (count == 9)
							{
								maisuu = qtyOfCards(state.kumi9);
							}
							else if (count == 10)
							{
								maisuu = qtyOfCards(state.kumi10);
							}
							else if (count == 11)
							{
								maisuu = qtyOfCards(state.kumi11);
							}
							else if (count == 12)
							{
								maisuu = qtyOfCards(state.kumi12);
							}
						}
					}
				}
			}
			if (count == 0)
			{
				point = setvalue_kaidan(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_kaidan(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_kaidan(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_kaidan(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_kaidan(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_kaidan(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_kaidan(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_kaidan(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_kaidan(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_kaidan(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_kaidan(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_kaidan(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_kaidan(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		else if (qtyOfCards2(pairtable) > 0)
		{

			//maisuu=0;

			for (pairmai = 5; pairmai > 1; pairmai--)
			{
				if (maisuu > 1)
					break;
				for (i = 0; i < 4; i++)
				{
					if (maisuu > 1)
						break;
					for (j = 1; j < 14; j++)
					{
						if (maisuu > 1)
							break;
						if (pairtable[i][j] == pairmai)
						{

							for (k = 0; k < 4; k++)
							{
								if (copytable[k][j] == 1 || copytable[k][j] == 2)
								{

									if (count == 0)
									{
										state.kumi0[k][j] = copytable[k][j];
									}
									else if (count == 1)
									{
										state.kumi1[k][j] = copytable[k][j];
									}
									else if (count == 2)
									{
										state.kumi2[k][j] = copytable[k][j];
									}
									else if (count == 3)
									{
										state.kumi3[k][j] = copytable[k][j];
									}
									else if (count == 4)
									{
										;
										state.kumi4[k][j] = copytable[k][j];
									}
									else if (count == 5)
									{
										state.kumi5[k][j] = copytable[k][j];
									}
									else if (count == 6)
									{
										state.kumi6[k][j] = copytable[k][j];
									}
									else if (count == 7)
									{
										state.kumi7[k][j] = copytable[k][j];
									}
									else if (count == 8)
									{
										state.kumi8[k][j] = copytable[k][j];
									}
									else if (count == 9)
									{
										state.kumi9[k][j] = copytable[k][j];
									}
									else if (count == 10)
									{
										state.kumi10[k][j] = copytable[k][j];
									}
									else if (count == 11)
									{
										state.kumi11[k][j] = copytable[k][j];
									}
									else if (count == 12)
									{
										state.kumi12[k][j] = copytable[k][j];
									}

									copytable[k][j] = 0;

									count2 = count2 + 1;

									if (count2 == pairmai)
									{
										count2 = 0;
										break;
									}
								}
							}

							if (count == 0)
							{
								maisuu = qtyOfCards(state.kumi0);
							}
							else if (count == 1)
							{
								maisuu = qtyOfCards(state.kumi1);
							}
							else if (count == 2)
							{
								maisuu = qtyOfCards(state.kumi2);
							}
							else if (count == 3)
							{
								maisuu = qtyOfCards(state.kumi3);
							}
							else if (count == 4)
							{
								maisuu = qtyOfCards(state.kumi4);
							}
							else if (count == 5)
							{
								maisuu = qtyOfCards(state.kumi5);
							}
							else if (count == 6)
							{
								maisuu = qtyOfCards(state.kumi6);
							}
							else if (count == 7)
							{
								maisuu = qtyOfCards(state.kumi7);
							}
							else if (count == 8)
							{
								maisuu = qtyOfCards(state.kumi8);
							}
							else if (count == 9)
							{
								maisuu = qtyOfCards(state.kumi9);
							}
							else if (count == 10)
							{
								maisuu = qtyOfCards(state.kumi10);
							}
							else if (count == 11)
							{
								maisuu = qtyOfCards(state.kumi11);
							}
							else if (count == 12)
							{
								maisuu = qtyOfCards(state.kumi12);
							}
						}
					}
				}
			}
			if (count == 0)
			{
				point = setvalue_pair(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_pair(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_pair(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_pair(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_pair(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_pair(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_pair(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_pair(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_pair(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_pair(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_pair(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_pair(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_pair(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		else if (qtyOfCards(copytable) > 0)
		{

			for (i = 0; i < 4; i++)
			{
				if (maisuu == 1)
					break;
				for (j = 1; j < 14; j++)
				{
					if (maisuu == 1)
						break;
					if (copytable[i][j] == 1 || copytable[i][j] == 2)
					{

						if (count == 0)
						{
							state.kumi0[i][j] = copytable[i][j];
						}
						else if (count == 1)
						{
							state.kumi1[i][j] = copytable[i][j];
						}
						else if (count == 2)
						{
							state.kumi2[i][j] = copytable[i][j];
						}
						else if (count == 3)
						{
							state.kumi3[i][j] = copytable[i][j];
						}
						else if (count == 4)
						{
							state.kumi4[i][j] = copytable[i][j];
						}
						else if (count == 5)
						{
							state.kumi5[i][j] = copytable[i][j];
						}
						else if (count == 6)
						{
							state.kumi6[i][j] = copytable[i][j];
						}
						else if (count == 7)
						{
							state.kumi7[i][j] = copytable[i][j];
						}
						else if (count == 8)
						{
							state.kumi8[i][j] = copytable[i][j];
						}
						else if (count == 9)
						{
							state.kumi9[i][j] = copytable[i][j];
						}
						else if (count == 10)
						{
							state.kumi10[i][j] = copytable[i][j];
						}
						else if (count == 11)
						{
							state.kumi11[i][j] = copytable[i][j];
						}
						else if (count == 12)
						{
							state.kumi12[i][j] = copytable[i][j];
						}

						copytable[i][j] = 0;

						if (count == 0)
						{
							maisuu = qtyOfCards(state.kumi0);
						}
						else if (count == 1)
						{
							maisuu = qtyOfCards(state.kumi1);
						}
						else if (count == 2)
						{
							maisuu = qtyOfCards(state.kumi2);
						}
						else if (count == 3)
						{
							maisuu = qtyOfCards(state.kumi3);
						}
						else if (count == 4)
						{
							maisuu = qtyOfCards(state.kumi4);
						}
						else if (count == 5)
						{
							maisuu = qtyOfCards(state.kumi5);
						}
						else if (count == 6)
						{
							maisuu = qtyOfCards(state.kumi6);
						}
						else if (count == 7)
						{
							maisuu = qtyOfCards(state.kumi7);
						}
						else if (count == 8)
						{
							maisuu = qtyOfCards(state.kumi8);
						}
						else if (count == 9)
						{
							maisuu = qtyOfCards(state.kumi9);
						}
						else if (count == 10)
						{
							maisuu = qtyOfCards(state.kumi10);
						}
						else if (count == 11)
						{
							maisuu = qtyOfCards(state.kumi11);
						}
						else if (count == 12)
						{
							maisuu = qtyOfCards(state.kumi12);
						}
					}
				}
			}

			if (maisuu == 0 && joker_check(copytable) == 2)
			{
				if (count == 0)
				{
					state.kumi0[4][1] = 2;
				}
				else if (count == 1)
				{
					state.kumi1[4][1] = 2;
				}
				else if (count == 2)
				{
					state.kumi2[4][1] = 2;
				}
				else if (count == 3)
				{
					state.kumi3[4][1] = 2;
				}
				else if (count == 4)
				{
					state.kumi4[4][1] = 2;
				}
				else if (count == 5)
				{
					state.kumi5[4][1] = 2;
				}
				else if (count == 6)
				{
					state.kumi6[4][1] = 2;
				}
				else if (count == 7)
				{
					state.kumi7[4][1] = 2;
				}
				else if (count == 8)
				{
					state.kumi8[4][1] = 2;
				}
				else if (count == 9)
				{
					state.kumi9[4][1] = 2;
				}
				else if (count == 10)
				{
					state.kumi10[4][1] = 2;
				}
				else if (count == 11)
				{
					state.kumi11[4][1] = 2;
				}
				else if (count == 12)
				{
					state.kumi12[4][1] = 2;
				}
				copytable[4][1] = 0;
			}

			if (count == 0)
			{
				point = setvalue_single(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_single(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_single(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_single(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_single(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_single(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_single(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_single(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_single(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_single(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_single(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_single(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_single(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		/*printf("kumi:%d   point:%d\n",count,point);
		
						for(i=0;i<5;i++){
    	 					for(j=0;j<15;j++){
      						
    	 						if(count==0){
    	 							printf("%i ",state.kumi0[i][j]);
								}else if(count==1){
									printf("%i ",state.kumi1[i][j]);
								}else if(count==2){
									printf("%i ",state.kumi2[i][j]);
								}else if(count==3){
									printf("%i ",state.kumi3[i][j]);
								}else if(count==4){
									printf("%i ",state.kumi4[i][j]);
								}else if(count==5){
									printf("%i ",state.kumi5[i][j]);
								}else if(count==6){
									printf("%i ",state.kumi6[i][j]);
								}else if(count==7){
									printf("%i ",state.kumi7[i][j]);
								}else if(count==8){
									printf("%i ",state.kumi8[i][j]);
								}else if(count==9){
									printf("%i ",state.kumi9[i][j]);
								}else if(count==10){
									printf("%i ",state.kumi10[i][j]);
								}
    						}
    						printf("\n");

						}

						printf("\n");*/

		count++;
	}
	setvalue_hands();
}

int bunkatsu_noprint2_a(int table[8][15])
{
	int i, j, k, count = 0, point = 0, totalpoint = 0;
	int kaidantable[8][15] = {{0}};
	int pairtable[8][15] = {{0}};
	int copytable[8][15] = {{0}};
	int kaidanlen = 0;
	int pairmai = 0;
	int maisuu = 0;
	int c = 0, kumisuu = 0;
	int count2 = 0;

	//ペア→階段→単体

	clearTable(state.kumi0); //追加.組を保存する配列を一試合ごとにリセット。
	clearTable(state.kumi1);
	clearTable(state.kumi2);
	clearTable(state.kumi3);
	clearTable(state.kumi4);
	clearTable(state.kumi5);
	clearTable(state.kumi6);
	clearTable(state.kumi7);
	clearTable(state.kumi8);
	clearTable(state.kumi9);
	clearTable(state.kumi10);
	clearTable(state.kumi11);
	clearTable(state.kumi12);
	/*
						for(i=0;i<14;i++){
							clearkumi_info(i);
						}
						*/
	clearkumi_info2();
	for (i = 0; i < 5; i++)
	{
		for (j = 0; j < 15; j++)
		{
			copytable[i][j] = table[i][j]; //copytableの作成
		}
	}

	while (qtyOfCards(copytable) > 0)
	{

		maisuu = 0; //リセット

		for (i = 0; i < 4; i++)
		{ //階段tableの作成
			for (j = 13 - 1; j > 0; j--)
			{
				if (copytable[i][j] == 1 || copytable[i][j] == 2)
				{
					kaidantable[i][j] = kaidantable[i][j + 1] + 1;
				}
				else
				{
					kaidantable[i][j] = 0;
				}
			}
		}

		for (i = 0; i < 4; i++)
		{ //pairtableの作成
			for (j = 1; j < 14 - 1; j++)
			{
				if (copytable[i][j] == 1 || copytable[i][j] == 2)
				{
					pairtable[i][j] = copytable[0][j] + copytable[1][j] + copytable[2][j] + copytable[3][j];
					if (copytable[0][j] == 2 || copytable[1][j] == 2 || copytable[2][j] == 2 || copytable[3][j] == 2)
					{
						pairtable[i][j] = pairtable[i][j] - 1;
					}
				}
				else
				{
					pairtable[i][j] = 0;
				}
			}
		}

		for (i = 0; i < 4; i++)
		{ //test
			for (j = 1; j < 14; j++)
			{
				if (kaidantable[i][j] >= 5)
					kaidantable[i][j] = 4;
			}
		}

		for (i = 0; i < 4; i++)
		{ //test
			for (j = 1; j < 14; j++)
			{
				if (pairtable[i][j] == 4)
					pairtable[i][j] = 3;
			}
		}

		if (qtyOfCards2(pairtable) > 0)
		{

			//maisuu=0;

			for (pairmai = 5; pairmai > 1; pairmai--)
			{
				if (maisuu > 1)
					break;
				for (i = 0; i < 4; i++)
				{
					if (maisuu > 1)
						break;
					for (j = 1; j < 14; j++)
					{
						if (maisuu > 1)
							break;
						if (pairtable[i][j] == pairmai)
						{

							for (k = 0; k < 4; k++)
							{
								if (copytable[k][j] == 1 || copytable[k][j] == 2)
								{

									if (count == 0)
									{
										state.kumi0[k][j] = copytable[k][j];
									}
									else if (count == 1)
									{
										state.kumi1[k][j] = copytable[k][j];
									}
									else if (count == 2)
									{
										state.kumi2[k][j] = copytable[k][j];
									}
									else if (count == 3)
									{
										state.kumi3[k][j] = copytable[k][j];
									}
									else if (count == 4)
									{
										;
										state.kumi4[k][j] = copytable[k][j];
									}
									else if (count == 5)
									{
										state.kumi5[k][j] = copytable[k][j];
									}
									else if (count == 6)
									{
										state.kumi6[k][j] = copytable[k][j];
									}
									else if (count == 7)
									{
										state.kumi7[k][j] = copytable[k][j];
									}
									else if (count == 8)
									{
										state.kumi8[k][j] = copytable[k][j];
									}
									else if (count == 9)
									{
										state.kumi9[k][j] = copytable[k][j];
									}
									else if (count == 10)
									{
										state.kumi10[k][j] = copytable[k][j];
									}
									else if (count == 11)
									{
										state.kumi11[k][j] = copytable[k][j];
									}
									else if (count == 12)
									{
										state.kumi12[k][j] = copytable[k][j];
									}

									copytable[k][j] = 0;

									count2 = count2 + 1;

									if (count2 == pairmai)
									{
										count2 = 0;
										break;
									}
								}
							}

							if (count == 0)
							{
								maisuu = qtyOfCards(state.kumi0);
							}
							else if (count == 1)
							{
								maisuu = qtyOfCards(state.kumi1);
							}
							else if (count == 2)
							{
								maisuu = qtyOfCards(state.kumi2);
							}
							else if (count == 3)
							{
								maisuu = qtyOfCards(state.kumi3);
							}
							else if (count == 4)
							{
								maisuu = qtyOfCards(state.kumi4);
							}
							else if (count == 5)
							{
								maisuu = qtyOfCards(state.kumi5);
							}
							else if (count == 6)
							{
								maisuu = qtyOfCards(state.kumi6);
							}
							else if (count == 7)
							{
								maisuu = qtyOfCards(state.kumi7);
							}
							else if (count == 8)
							{
								maisuu = qtyOfCards(state.kumi8);
							}
							else if (count == 9)
							{
								maisuu = qtyOfCards(state.kumi9);
							}
							else if (count == 10)
							{
								maisuu = qtyOfCards(state.kumi10);
							}
							else if (count == 11)
							{
								maisuu = qtyOfCards(state.kumi11);
							}
							else if (count == 12)
							{
								maisuu = qtyOfCards(state.kumi12);
							}
						}
					}
				}
			}
			if (count == 0)
			{
				point = setvalue_pair(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_pair(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_pair(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_pair(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_pair(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_pair(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_pair(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_pair(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_pair(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_pair(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_pair(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_pair(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_pair(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		else if (qtyOfCards3(kaidantable) > 0)
		{

			for (kaidanlen = 11; kaidanlen > 2; kaidanlen--)
			{
				if (maisuu > 2)
					break;
				for (i = 0; i < 4; i++)
				{
					if (maisuu > 2)
						break;
					for (j = 1; j < 14; j++)
					{
						if (maisuu > 2)
							break;
						if (kaidantable[i][j] == kaidanlen)
						{
							for (k = 0; k < kaidanlen; k++)
							{

								if (count == 0)
								{
									state.kumi0[i][j + k] = copytable[i][j + k];
								}
								else if (count == 1)
								{
									state.kumi1[i][j + k] = copytable[i][j + k];
								}
								else if (count == 2)
								{
									state.kumi2[i][j + k] = copytable[i][j + k];
								}
								else if (count == 3)
								{
									state.kumi3[i][j + k] = copytable[i][j + k];
								}
								else if (count == 4)
								{
									state.kumi4[i][j + k] = copytable[i][j + k];
								}
								else if (count == 5)
								{
									state.kumi5[i][j + k] = copytable[i][j + k];
								}
								else if (count == 6)
								{
									state.kumi6[i][j + k] = copytable[i][j + k];
								}
								else if (count == 7)
								{
									state.kumi7[i][j + k] = copytable[i][j + k];
								}
								else if (count == 8)
								{
									state.kumi8[i][j + k] = copytable[i][j + k];
								}
								else if (count == 9)
								{
									state.kumi9[i][j + k] = copytable[i][j + k];
								}
								else if (count == 10)
								{
									state.kumi10[i][j + k] = copytable[i][j + k];
								}
								else if (count == 11)
								{
									state.kumi11[i][j + k] = copytable[i][j + k];
								}
								else if (count == 12)
								{
									state.kumi12[i][j + k] = copytable[i][j + k];
								}
								copytable[i][j + k] = 0;
							}

							if (count == 0)
							{
								maisuu = qtyOfCards(state.kumi0);
							}
							else if (count == 1)
							{
								maisuu = qtyOfCards(state.kumi1);
							}
							else if (count == 2)
							{
								maisuu = qtyOfCards(state.kumi2);
							}
							else if (count == 3)
							{
								maisuu = qtyOfCards(state.kumi3);
							}
							else if (count == 4)
							{
								maisuu = qtyOfCards(state.kumi4);
							}
							else if (count == 5)
							{
								maisuu = qtyOfCards(state.kumi5);
							}
							else if (count == 6)
							{
								maisuu = qtyOfCards(state.kumi6);
							}
							else if (count == 7)
							{
								maisuu = qtyOfCards(state.kumi7);
							}
							else if (count == 8)
							{
								maisuu = qtyOfCards(state.kumi8);
							}
							else if (count == 9)
							{
								maisuu = qtyOfCards(state.kumi9);
							}
							else if (count == 10)
							{
								maisuu = qtyOfCards(state.kumi10);
							}
							else if (count == 11)
							{
								maisuu = qtyOfCards(state.kumi11);
							}
							else if (count == 12)
							{
								maisuu = qtyOfCards(state.kumi12);
							}
						}
					}
				}
			}
			if (count == 0)
			{
				point = setvalue_kaidan(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_kaidan(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_kaidan(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_kaidan(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_kaidan(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_kaidan(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_kaidan(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_kaidan(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_kaidan(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_kaidan(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_kaidan(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_kaidan(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_kaidan(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		else if (qtyOfCards(copytable) > 0)
		{

			for (i = 0; i < 4; i++)
			{
				if (maisuu == 1)
					break;
				for (j = 1; j < 14; j++)
				{
					if (maisuu == 1)
						break;
					if (copytable[i][j] == 1 || copytable[i][j] == 2)
					{

						if (count == 0)
						{
							state.kumi0[i][j] = copytable[i][j];
						}
						else if (count == 1)
						{
							state.kumi1[i][j] = copytable[i][j];
						}
						else if (count == 2)
						{
							state.kumi2[i][j] = copytable[i][j];
						}
						else if (count == 3)
						{
							state.kumi3[i][j] = copytable[i][j];
						}
						else if (count == 4)
						{
							state.kumi4[i][j] = copytable[i][j];
						}
						else if (count == 5)
						{
							state.kumi5[i][j] = copytable[i][j];
						}
						else if (count == 6)
						{
							state.kumi6[i][j] = copytable[i][j];
						}
						else if (count == 7)
						{
							state.kumi7[i][j] = copytable[i][j];
						}
						else if (count == 8)
						{
							state.kumi8[i][j] = copytable[i][j];
						}
						else if (count == 9)
						{
							state.kumi9[i][j] = copytable[i][j];
						}
						else if (count == 10)
						{
							state.kumi10[i][j] = copytable[i][j];
						}
						else if (count == 11)
						{
							state.kumi11[i][j] = copytable[i][j];
						}
						else if (count == 12)
						{
							state.kumi12[i][j] = copytable[i][j];
						}

						copytable[i][j] = 0;

						if (count == 0)
						{
							maisuu = qtyOfCards(state.kumi0);
						}
						else if (count == 1)
						{
							maisuu = qtyOfCards(state.kumi1);
						}
						else if (count == 2)
						{
							maisuu = qtyOfCards(state.kumi2);
						}
						else if (count == 3)
						{
							maisuu = qtyOfCards(state.kumi3);
						}
						else if (count == 4)
						{
							maisuu = qtyOfCards(state.kumi4);
						}
						else if (count == 5)
						{
							maisuu = qtyOfCards(state.kumi5);
						}
						else if (count == 6)
						{
							maisuu = qtyOfCards(state.kumi6);
						}
						else if (count == 7)
						{
							maisuu = qtyOfCards(state.kumi7);
						}
						else if (count == 8)
						{
							maisuu = qtyOfCards(state.kumi8);
						}
						else if (count == 9)
						{
							maisuu = qtyOfCards(state.kumi9);
						}
						else if (count == 10)
						{
							maisuu = qtyOfCards(state.kumi10);
						}
						else if (count == 11)
						{
							maisuu = qtyOfCards(state.kumi11);
						}
						else if (count == 12)
						{
							maisuu = qtyOfCards(state.kumi12);
						}
					}
				}
			}

			if (maisuu == 0 && joker_check(copytable) == 2)
			{
				if (count == 0)
				{
					state.kumi0[4][1] = 2;
				}
				else if (count == 1)
				{
					state.kumi1[4][1] = 2;
				}
				else if (count == 2)
				{
					state.kumi2[4][1] = 2;
				}
				else if (count == 3)
				{
					state.kumi3[4][1] = 2;
				}
				else if (count == 4)
				{
					state.kumi4[4][1] = 2;
				}
				else if (count == 5)
				{
					state.kumi5[4][1] = 2;
				}
				else if (count == 6)
				{
					state.kumi6[4][1] = 2;
				}
				else if (count == 7)
				{
					state.kumi7[4][1] = 2;
				}
				else if (count == 8)
				{
					state.kumi8[4][1] = 2;
				}
				else if (count == 9)
				{
					state.kumi9[4][1] = 2;
				}
				else if (count == 10)
				{
					state.kumi10[4][1] = 2;
				}
				else if (count == 11)
				{
					state.kumi11[4][1] = 2;
				}
				else if (count == 12)
				{
					state.kumi12[4][1] = 2;
				}
				copytable[4][1] = 0;
			}

			if (count == 0)
			{
				point = setvalue_single(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_single(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_single(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_single(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_single(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_single(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_single(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_single(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_single(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_single(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_single(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_single(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_single(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		/*printf("kumi:%d   point:%d\n",count,point);
		
						for(i=0;i<5;i++){
    	 					for(j=0;j<15;j++){
      						
    	 						if(count==0){
    	 							printf("%i ",state.kumi0[i][j]);
								}else if(count==1){
									printf("%i ",state.kumi1[i][j]);
								}else if(count==2){
									printf("%i ",state.kumi2[i][j]);
								}else if(count==3){
									printf("%i ",state.kumi3[i][j]);
								}else if(count==4){
									printf("%i ",state.kumi4[i][j]);
								}else if(count==5){
									printf("%i ",state.kumi5[i][j]);
								}else if(count==6){
									printf("%i ",state.kumi6[i][j]);
								}else if(count==7){
									printf("%i ",state.kumi7[i][j]);
								}else if(count==8){
									printf("%i ",state.kumi8[i][j]);
								}else if(count==9){
									printf("%i ",state.kumi9[i][j]);
								}else if(count==10){
									printf("%i ",state.kumi10[i][j]);
								}
    						}
    						printf("\n");

						}

						printf("\n");*/

		count++;
	}
	setvalue_hands();
}

int bunkatsu_noprint_c(int table[8][15])
{
	int i, j, k, count = 0, point = 0, totalpoint = 0;
	int kaidantable[8][15] = {{0}};
	int pairtable[8][15] = {{0}};
	int copytable[8][15] = {{0}};
	int kaidanlen = 0;
	int pairmai = 0;
	int maisuu = 0;
	int c = 0, kumisuu = 0;
	int count2 = 0;

	//階段→ペア→単体

	clearTable(state.kumi0); //追加.組を保存する配列を一試合ごとにリセット。
	clearTable(state.kumi1);
	clearTable(state.kumi2);
	clearTable(state.kumi3);
	clearTable(state.kumi4);
	clearTable(state.kumi5);
	clearTable(state.kumi6);
	clearTable(state.kumi7);
	clearTable(state.kumi8);
	clearTable(state.kumi9);
	clearTable(state.kumi10);
	clearTable(state.kumi11);
	clearTable(state.kumi12);
	/*
						for(i=0;i<14;i++){
							clearkumi_info(i);
						}
						*/
	clearkumi_info2();
	for (i = 0; i < 5; i++)
	{
		for (j = 0; j < 15; j++)
		{
			copytable[i][j] = table[i][j]; //copytableの作成
		}
	}

	while (qtyOfCards(copytable) > 0)
	{

		maisuu = 0; //リセット

		for (i = 0; i < 4; i++)
		{ //階段tableの作成
			for (j = 13; j > 0; j--)
			{
				if (copytable[i][j] == 1 || copytable[i][j] == 2)
				{
					kaidantable[i][j] = kaidantable[i][j + 1] + 1;
				}
				else
				{
					kaidantable[i][j] = 0;
				}
			}
		}

		for (i = 0; i < 4; i++)
		{ //pairtableの作成
			for (j = 1; j < 14; j++)
			{
				if (copytable[i][j] == 1 || copytable[i][j] == 2)
				{
					pairtable[i][j] = copytable[0][j] + copytable[1][j] + copytable[2][j] + copytable[3][j];
					if (copytable[0][j] == 2 || copytable[1][j] == 2 || copytable[2][j] == 2 || copytable[3][j] == 2)
					{
						pairtable[i][j] = pairtable[i][j] - 1;
					}
				}
				else
				{
					pairtable[i][j] = 0;
				}
			}
		}
		/*
						for(i=0;i<4;i++){//test
							for(j=1;j<14;j++){
								if(kaidantable[i][j]>=5)kaidantable[i][j]=4;
							}
						}
		*/
		/*
						for(i=0;i<4;i++){//test
							for(j=1;j<14;j++){
								if(pairtable[i][j]==4)pairtable[i][j]=3;
							}
						}
		*/

		if (qtyOfCards3(kaidantable) > 0)
		{

			for (kaidanlen = 11; kaidanlen > 2; kaidanlen--)
			{
				if (maisuu > 2)
					break;
				for (i = 0; i < 4; i++)
				{
					if (maisuu > 2)
						break;
					for (j = 1; j < 14; j++)
					{
						if (maisuu > 2)
							break;
						if (kaidantable[i][j] == kaidanlen)
						{
							for (k = 0; k < kaidanlen; k++)
							{

								if (count == 0)
								{
									state.kumi0[i][j + k] = copytable[i][j + k];
								}
								else if (count == 1)
								{
									state.kumi1[i][j + k] = copytable[i][j + k];
								}
								else if (count == 2)
								{
									state.kumi2[i][j + k] = copytable[i][j + k];
								}
								else if (count == 3)
								{
									state.kumi3[i][j + k] = copytable[i][j + k];
								}
								else if (count == 4)
								{
									state.kumi4[i][j + k] = copytable[i][j + k];
								}
								else if (count == 5)
								{
									state.kumi5[i][j + k] = copytable[i][j + k];
								}
								else if (count == 6)
								{
									state.kumi6[i][j + k] = copytable[i][j + k];
								}
								else if (count == 7)
								{
									state.kumi7[i][j + k] = copytable[i][j + k];
								}
								else if (count == 8)
								{
									state.kumi8[i][j + k] = copytable[i][j + k];
								}
								else if (count == 9)
								{
									state.kumi9[i][j + k] = copytable[i][j + k];
								}
								else if (count == 10)
								{
									state.kumi10[i][j + k] = copytable[i][j + k];
								}
								else if (count == 11)
								{
									state.kumi11[i][j + k] = copytable[i][j + k];
								}
								else if (count == 12)
								{
									state.kumi12[i][j + k] = copytable[i][j + k];
								}
								copytable[i][j + k] = 0;
							}

							if (count == 0)
							{
								maisuu = qtyOfCards(state.kumi0);
							}
							else if (count == 1)
							{
								maisuu = qtyOfCards(state.kumi1);
							}
							else if (count == 2)
							{
								maisuu = qtyOfCards(state.kumi2);
							}
							else if (count == 3)
							{
								maisuu = qtyOfCards(state.kumi3);
							}
							else if (count == 4)
							{
								maisuu = qtyOfCards(state.kumi4);
							}
							else if (count == 5)
							{
								maisuu = qtyOfCards(state.kumi5);
							}
							else if (count == 6)
							{
								maisuu = qtyOfCards(state.kumi6);
							}
							else if (count == 7)
							{
								maisuu = qtyOfCards(state.kumi7);
							}
							else if (count == 8)
							{
								maisuu = qtyOfCards(state.kumi8);
							}
							else if (count == 9)
							{
								maisuu = qtyOfCards(state.kumi9);
							}
							else if (count == 10)
							{
								maisuu = qtyOfCards(state.kumi10);
							}
							else if (count == 11)
							{
								maisuu = qtyOfCards(state.kumi11);
							}
							else if (count == 12)
							{
								maisuu = qtyOfCards(state.kumi12);
							}
						}
					}
				}
			}
			if (count == 0)
			{
				point = setvalue_kaidan(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_kaidan(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_kaidan(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_kaidan(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_kaidan(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_kaidan(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_kaidan(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_kaidan(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_kaidan(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_kaidan(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_kaidan(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_kaidan(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_kaidan(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		else if (qtyOfCards2(pairtable) > 0)
		{

			//maisuu=0;

			for (pairmai = 5; pairmai > 1; pairmai--)
			{
				if (maisuu > 1)
					break;
				for (i = 0; i < 4; i++)
				{
					if (maisuu > 1)
						break;
					for (j = 1; j < 14; j++)
					{
						if (maisuu > 1)
							break;
						if (pairtable[i][j] == pairmai)
						{

							for (k = 0; k < 4; k++)
							{
								if (copytable[k][j] == 1 || copytable[k][j] == 2)
								{

									if (count == 0)
									{
										state.kumi0[k][j] = copytable[k][j];
									}
									else if (count == 1)
									{
										state.kumi1[k][j] = copytable[k][j];
									}
									else if (count == 2)
									{
										state.kumi2[k][j] = copytable[k][j];
									}
									else if (count == 3)
									{
										state.kumi3[k][j] = copytable[k][j];
									}
									else if (count == 4)
									{
										;
										state.kumi4[k][j] = copytable[k][j];
									}
									else if (count == 5)
									{
										state.kumi5[k][j] = copytable[k][j];
									}
									else if (count == 6)
									{
										state.kumi6[k][j] = copytable[k][j];
									}
									else if (count == 7)
									{
										state.kumi7[k][j] = copytable[k][j];
									}
									else if (count == 8)
									{
										state.kumi8[k][j] = copytable[k][j];
									}
									else if (count == 9)
									{
										state.kumi9[k][j] = copytable[k][j];
									}
									else if (count == 10)
									{
										state.kumi10[k][j] = copytable[k][j];
									}
									else if (count == 11)
									{
										state.kumi11[k][j] = copytable[k][j];
									}
									else if (count == 12)
									{
										state.kumi12[k][j] = copytable[k][j];
									}

									copytable[k][j] = 0;

									count2 = count2 + 1;

									if (count2 == pairmai)
									{
										count2 = 0;
										break;
									}
								}
							}

							if (count == 0)
							{
								maisuu = qtyOfCards(state.kumi0);
							}
							else if (count == 1)
							{
								maisuu = qtyOfCards(state.kumi1);
							}
							else if (count == 2)
							{
								maisuu = qtyOfCards(state.kumi2);
							}
							else if (count == 3)
							{
								maisuu = qtyOfCards(state.kumi3);
							}
							else if (count == 4)
							{
								maisuu = qtyOfCards(state.kumi4);
							}
							else if (count == 5)
							{
								maisuu = qtyOfCards(state.kumi5);
							}
							else if (count == 6)
							{
								maisuu = qtyOfCards(state.kumi6);
							}
							else if (count == 7)
							{
								maisuu = qtyOfCards(state.kumi7);
							}
							else if (count == 8)
							{
								maisuu = qtyOfCards(state.kumi8);
							}
							else if (count == 9)
							{
								maisuu = qtyOfCards(state.kumi9);
							}
							else if (count == 10)
							{
								maisuu = qtyOfCards(state.kumi10);
							}
							else if (count == 11)
							{
								maisuu = qtyOfCards(state.kumi11);
							}
							else if (count == 12)
							{
								maisuu = qtyOfCards(state.kumi12);
							}
						}
					}
				}
			}
			if (count == 0)
			{
				point = setvalue_pair(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_pair(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_pair(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_pair(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_pair(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_pair(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_pair(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_pair(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_pair(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_pair(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_pair(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_pair(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_pair(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		else if (qtyOfCards(copytable) > 0)
		{

			for (i = 0; i < 4; i++)
			{
				if (maisuu == 1)
					break;
				for (j = 1; j < 14; j++)
				{
					if (maisuu == 1)
						break;
					if (copytable[i][j] == 1 || copytable[i][j] == 2)
					{

						if (count == 0)
						{
							state.kumi0[i][j] = copytable[i][j];
						}
						else if (count == 1)
						{
							state.kumi1[i][j] = copytable[i][j];
						}
						else if (count == 2)
						{
							state.kumi2[i][j] = copytable[i][j];
						}
						else if (count == 3)
						{
							state.kumi3[i][j] = copytable[i][j];
						}
						else if (count == 4)
						{
							state.kumi4[i][j] = copytable[i][j];
						}
						else if (count == 5)
						{
							state.kumi5[i][j] = copytable[i][j];
						}
						else if (count == 6)
						{
							state.kumi6[i][j] = copytable[i][j];
						}
						else if (count == 7)
						{
							state.kumi7[i][j] = copytable[i][j];
						}
						else if (count == 8)
						{
							state.kumi8[i][j] = copytable[i][j];
						}
						else if (count == 9)
						{
							state.kumi9[i][j] = copytable[i][j];
						}
						else if (count == 10)
						{
							state.kumi10[i][j] = copytable[i][j];
						}
						else if (count == 11)
						{
							state.kumi11[i][j] = copytable[i][j];
						}
						else if (count == 12)
						{
							state.kumi12[i][j] = copytable[i][j];
						}

						copytable[i][j] = 0;

						if (count == 0)
						{
							maisuu = qtyOfCards(state.kumi0);
						}
						else if (count == 1)
						{
							maisuu = qtyOfCards(state.kumi1);
						}
						else if (count == 2)
						{
							maisuu = qtyOfCards(state.kumi2);
						}
						else if (count == 3)
						{
							maisuu = qtyOfCards(state.kumi3);
						}
						else if (count == 4)
						{
							maisuu = qtyOfCards(state.kumi4);
						}
						else if (count == 5)
						{
							maisuu = qtyOfCards(state.kumi5);
						}
						else if (count == 6)
						{
							maisuu = qtyOfCards(state.kumi6);
						}
						else if (count == 7)
						{
							maisuu = qtyOfCards(state.kumi7);
						}
						else if (count == 8)
						{
							maisuu = qtyOfCards(state.kumi8);
						}
						else if (count == 9)
						{
							maisuu = qtyOfCards(state.kumi9);
						}
						else if (count == 10)
						{
							maisuu = qtyOfCards(state.kumi10);
						}
						else if (count == 11)
						{
							maisuu = qtyOfCards(state.kumi11);
						}
						else if (count == 12)
						{
							maisuu = qtyOfCards(state.kumi12);
						}
					}
				}
			}

			if (maisuu == 0 && joker_check(copytable) == 2)
			{
				if (count == 0)
				{
					state.kumi0[4][1] = 2;
				}
				else if (count == 1)
				{
					state.kumi1[4][1] = 2;
				}
				else if (count == 2)
				{
					state.kumi2[4][1] = 2;
				}
				else if (count == 3)
				{
					state.kumi3[4][1] = 2;
				}
				else if (count == 4)
				{
					state.kumi4[4][1] = 2;
				}
				else if (count == 5)
				{
					state.kumi5[4][1] = 2;
				}
				else if (count == 6)
				{
					state.kumi6[4][1] = 2;
				}
				else if (count == 7)
				{
					state.kumi7[4][1] = 2;
				}
				else if (count == 8)
				{
					state.kumi8[4][1] = 2;
				}
				else if (count == 9)
				{
					state.kumi9[4][1] = 2;
				}
				else if (count == 10)
				{
					state.kumi10[4][1] = 2;
				}
				else if (count == 11)
				{
					state.kumi11[4][1] = 2;
				}
				else if (count == 12)
				{
					state.kumi12[4][1] = 2;
				}
				copytable[4][1] = 0;
			}

			if (count == 0)
			{
				point = setvalue_single(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_single(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_single(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_single(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_single(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_single(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_single(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_single(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_single(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_single(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_single(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_single(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_single(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		/*printf("kumi:%d   point:%d\n",count,point);
		
						for(i=0;i<5;i++){
    	 					for(j=0;j<15;j++){
      						
    	 						if(count==0){
    	 							printf("%i ",state.kumi0[i][j]);
								}else if(count==1){
									printf("%i ",state.kumi1[i][j]);
								}else if(count==2){
									printf("%i ",state.kumi2[i][j]);
								}else if(count==3){
									printf("%i ",state.kumi3[i][j]);
								}else if(count==4){
									printf("%i ",state.kumi4[i][j]);
								}else if(count==5){
									printf("%i ",state.kumi5[i][j]);
								}else if(count==6){
									printf("%i ",state.kumi6[i][j]);
								}else if(count==7){
									printf("%i ",state.kumi7[i][j]);
								}else if(count==8){
									printf("%i ",state.kumi8[i][j]);
								}else if(count==9){
									printf("%i ",state.kumi9[i][j]);
								}else if(count==10){
									printf("%i ",state.kumi10[i][j]);
								}
    						}
    						printf("\n");

						}

						printf("\n");*/

		count++;
	}
	setvalue_hands();
}

int bunkatsu_noprint2_c(int table[8][15])
{
	int i, j, k, count = 0, point = 0, totalpoint = 0;
	int kaidantable[8][15] = {{0}};
	int pairtable[8][15] = {{0}};
	int copytable[8][15] = {{0}};
	int kaidanlen = 0;
	int pairmai = 0;
	int maisuu = 0;
	int c = 0, kumisuu = 0;
	int count2 = 0;

	//ペア→階段→単体

	clearTable(state.kumi0); //追加.組を保存する配列を一試合ごとにリセット。
	clearTable(state.kumi1);
	clearTable(state.kumi2);
	clearTable(state.kumi3);
	clearTable(state.kumi4);
	clearTable(state.kumi5);
	clearTable(state.kumi6);
	clearTable(state.kumi7);
	clearTable(state.kumi8);
	clearTable(state.kumi9);
	clearTable(state.kumi10);
	clearTable(state.kumi11);
	clearTable(state.kumi12);
	/*
						for(i=0;i<14;i++){
							clearkumi_info(i);
						}
						*/
	clearkumi_info2();
	for (i = 0; i < 5; i++)
	{
		for (j = 0; j < 15; j++)
		{
			copytable[i][j] = table[i][j]; //copytableの作成
		}
	}

	while (qtyOfCards(copytable) > 0)
	{

		maisuu = 0; //リセット

		for (i = 0; i < 4; i++)
		{ //階段tableの作成
			for (j = 13; j > 0; j--)
			{
				if (copytable[i][j] == 1 || copytable[i][j] == 2)
				{
					kaidantable[i][j] = kaidantable[i][j + 1] + 1;
				}
				else
				{
					kaidantable[i][j] = 0;
				}
			}
		}

		for (i = 0; i < 4; i++)
		{ //pairtableの作成
			for (j = 1; j < 14; j++)
			{
				if (copytable[i][j] == 1 || copytable[i][j] == 2)
				{
					pairtable[i][j] = copytable[0][j] + copytable[1][j] + copytable[2][j] + copytable[3][j];
					if (copytable[0][j] == 2 || copytable[1][j] == 2 || copytable[2][j] == 2 || copytable[3][j] == 2)
					{
						pairtable[i][j] = pairtable[i][j] - 1;
					}
				}
				else
				{
					pairtable[i][j] = 0;
				}
			}
		}
		/*
						for(i=0;i<4;i++){//test
							for(j=1;j<14;j++){
								if(kaidantable[i][j]>=5)kaidantable[i][j]=4;
							}
						}
		*/
		/*
						for(i=0;i<4;i++){//test
							for(j=1;j<14;j++){
								if(pairtable[i][j]==4)pairtable[i][j]=3;
							}
						}
		*/

		if (qtyOfCards2(pairtable) > 0)
		{

			//maisuu=0;

			for (pairmai = 5; pairmai > 1; pairmai--)
			{
				if (maisuu > 1)
					break;
				for (i = 0; i < 4; i++)
				{
					if (maisuu > 1)
						break;
					for (j = 1; j < 14; j++)
					{
						if (maisuu > 1)
							break;
						if (pairtable[i][j] == pairmai)
						{

							for (k = 0; k < 4; k++)
							{
								if (copytable[k][j] == 1 || copytable[k][j] == 2)
								{

									if (count == 0)
									{
										state.kumi0[k][j] = copytable[k][j];
									}
									else if (count == 1)
									{
										state.kumi1[k][j] = copytable[k][j];
									}
									else if (count == 2)
									{
										state.kumi2[k][j] = copytable[k][j];
									}
									else if (count == 3)
									{
										state.kumi3[k][j] = copytable[k][j];
									}
									else if (count == 4)
									{
										;
										state.kumi4[k][j] = copytable[k][j];
									}
									else if (count == 5)
									{
										state.kumi5[k][j] = copytable[k][j];
									}
									else if (count == 6)
									{
										state.kumi6[k][j] = copytable[k][j];
									}
									else if (count == 7)
									{
										state.kumi7[k][j] = copytable[k][j];
									}
									else if (count == 8)
									{
										state.kumi8[k][j] = copytable[k][j];
									}
									else if (count == 9)
									{
										state.kumi9[k][j] = copytable[k][j];
									}
									else if (count == 10)
									{
										state.kumi10[k][j] = copytable[k][j];
									}
									else if (count == 11)
									{
										state.kumi11[k][j] = copytable[k][j];
									}
									else if (count == 12)
									{
										state.kumi12[k][j] = copytable[k][j];
									}

									copytable[k][j] = 0;

									count2 = count2 + 1;

									if (count2 == pairmai)
									{
										count2 = 0;
										break;
									}
								}
							}

							if (count == 0)
							{
								maisuu = qtyOfCards(state.kumi0);
							}
							else if (count == 1)
							{
								maisuu = qtyOfCards(state.kumi1);
							}
							else if (count == 2)
							{
								maisuu = qtyOfCards(state.kumi2);
							}
							else if (count == 3)
							{
								maisuu = qtyOfCards(state.kumi3);
							}
							else if (count == 4)
							{
								maisuu = qtyOfCards(state.kumi4);
							}
							else if (count == 5)
							{
								maisuu = qtyOfCards(state.kumi5);
							}
							else if (count == 6)
							{
								maisuu = qtyOfCards(state.kumi6);
							}
							else if (count == 7)
							{
								maisuu = qtyOfCards(state.kumi7);
							}
							else if (count == 8)
							{
								maisuu = qtyOfCards(state.kumi8);
							}
							else if (count == 9)
							{
								maisuu = qtyOfCards(state.kumi9);
							}
							else if (count == 10)
							{
								maisuu = qtyOfCards(state.kumi10);
							}
							else if (count == 11)
							{
								maisuu = qtyOfCards(state.kumi11);
							}
							else if (count == 12)
							{
								maisuu = qtyOfCards(state.kumi12);
							}
						}
					}
				}
			}
			if (count == 0)
			{
				point = setvalue_pair(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_pair(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_pair(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_pair(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_pair(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_pair(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_pair(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_pair(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_pair(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_pair(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_pair(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_pair(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_pair(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		else if (qtyOfCards3(kaidantable) > 0)
		{

			for (kaidanlen = 11; kaidanlen > 2; kaidanlen--)
			{
				if (maisuu > 2)
					break;
				for (i = 0; i < 4; i++)
				{
					if (maisuu > 2)
						break;
					for (j = 1; j < 14; j++)
					{
						if (maisuu > 2)
							break;
						if (kaidantable[i][j] == kaidanlen)
						{
							for (k = 0; k < kaidanlen; k++)
							{

								if (count == 0)
								{
									state.kumi0[i][j + k] = copytable[i][j + k];
								}
								else if (count == 1)
								{
									state.kumi1[i][j + k] = copytable[i][j + k];
								}
								else if (count == 2)
								{
									state.kumi2[i][j + k] = copytable[i][j + k];
								}
								else if (count == 3)
								{
									state.kumi3[i][j + k] = copytable[i][j + k];
								}
								else if (count == 4)
								{
									state.kumi4[i][j + k] = copytable[i][j + k];
								}
								else if (count == 5)
								{
									state.kumi5[i][j + k] = copytable[i][j + k];
								}
								else if (count == 6)
								{
									state.kumi6[i][j + k] = copytable[i][j + k];
								}
								else if (count == 7)
								{
									state.kumi7[i][j + k] = copytable[i][j + k];
								}
								else if (count == 8)
								{
									state.kumi8[i][j + k] = copytable[i][j + k];
								}
								else if (count == 9)
								{
									state.kumi9[i][j + k] = copytable[i][j + k];
								}
								else if (count == 10)
								{
									state.kumi10[i][j + k] = copytable[i][j + k];
								}
								else if (count == 11)
								{
									state.kumi11[i][j + k] = copytable[i][j + k];
								}
								else if (count == 12)
								{
									state.kumi12[i][j + k] = copytable[i][j + k];
								}
								copytable[i][j + k] = 0;
							}

							if (count == 0)
							{
								maisuu = qtyOfCards(state.kumi0);
							}
							else if (count == 1)
							{
								maisuu = qtyOfCards(state.kumi1);
							}
							else if (count == 2)
							{
								maisuu = qtyOfCards(state.kumi2);
							}
							else if (count == 3)
							{
								maisuu = qtyOfCards(state.kumi3);
							}
							else if (count == 4)
							{
								maisuu = qtyOfCards(state.kumi4);
							}
							else if (count == 5)
							{
								maisuu = qtyOfCards(state.kumi5);
							}
							else if (count == 6)
							{
								maisuu = qtyOfCards(state.kumi6);
							}
							else if (count == 7)
							{
								maisuu = qtyOfCards(state.kumi7);
							}
							else if (count == 8)
							{
								maisuu = qtyOfCards(state.kumi8);
							}
							else if (count == 9)
							{
								maisuu = qtyOfCards(state.kumi9);
							}
							else if (count == 10)
							{
								maisuu = qtyOfCards(state.kumi10);
							}
							else if (count == 11)
							{
								maisuu = qtyOfCards(state.kumi11);
							}
							else if (count == 12)
							{
								maisuu = qtyOfCards(state.kumi12);
							}
						}
					}
				}
			}
			if (count == 0)
			{
				point = setvalue_kaidan(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_kaidan(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_kaidan(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_kaidan(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_kaidan(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_kaidan(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_kaidan(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_kaidan(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_kaidan(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_kaidan(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_kaidan(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_kaidan(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_kaidan(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		else if (qtyOfCards(copytable) > 0)
		{

			for (i = 0; i < 4; i++)
			{
				if (maisuu == 1)
					break;
				for (j = 1; j < 14; j++)
				{
					if (maisuu == 1)
						break;
					if (copytable[i][j] == 1 || copytable[i][j] == 2)
					{

						if (count == 0)
						{
							state.kumi0[i][j] = copytable[i][j];
						}
						else if (count == 1)
						{
							state.kumi1[i][j] = copytable[i][j];
						}
						else if (count == 2)
						{
							state.kumi2[i][j] = copytable[i][j];
						}
						else if (count == 3)
						{
							state.kumi3[i][j] = copytable[i][j];
						}
						else if (count == 4)
						{
							state.kumi4[i][j] = copytable[i][j];
						}
						else if (count == 5)
						{
							state.kumi5[i][j] = copytable[i][j];
						}
						else if (count == 6)
						{
							state.kumi6[i][j] = copytable[i][j];
						}
						else if (count == 7)
						{
							state.kumi7[i][j] = copytable[i][j];
						}
						else if (count == 8)
						{
							state.kumi8[i][j] = copytable[i][j];
						}
						else if (count == 9)
						{
							state.kumi9[i][j] = copytable[i][j];
						}
						else if (count == 10)
						{
							state.kumi10[i][j] = copytable[i][j];
						}
						else if (count == 11)
						{
							state.kumi11[i][j] = copytable[i][j];
						}
						else if (count == 12)
						{
							state.kumi12[i][j] = copytable[i][j];
						}

						copytable[i][j] = 0;

						if (count == 0)
						{
							maisuu = qtyOfCards(state.kumi0);
						}
						else if (count == 1)
						{
							maisuu = qtyOfCards(state.kumi1);
						}
						else if (count == 2)
						{
							maisuu = qtyOfCards(state.kumi2);
						}
						else if (count == 3)
						{
							maisuu = qtyOfCards(state.kumi3);
						}
						else if (count == 4)
						{
							maisuu = qtyOfCards(state.kumi4);
						}
						else if (count == 5)
						{
							maisuu = qtyOfCards(state.kumi5);
						}
						else if (count == 6)
						{
							maisuu = qtyOfCards(state.kumi6);
						}
						else if (count == 7)
						{
							maisuu = qtyOfCards(state.kumi7);
						}
						else if (count == 8)
						{
							maisuu = qtyOfCards(state.kumi8);
						}
						else if (count == 9)
						{
							maisuu = qtyOfCards(state.kumi9);
						}
						else if (count == 10)
						{
							maisuu = qtyOfCards(state.kumi10);
						}
						else if (count == 11)
						{
							maisuu = qtyOfCards(state.kumi11);
						}
						else if (count == 12)
						{
							maisuu = qtyOfCards(state.kumi12);
						}
					}
				}
			}

			if (maisuu == 0 && joker_check(copytable) == 2)
			{
				if (count == 0)
				{
					state.kumi0[4][1] = 2;
				}
				else if (count == 1)
				{
					state.kumi1[4][1] = 2;
				}
				else if (count == 2)
				{
					state.kumi2[4][1] = 2;
				}
				else if (count == 3)
				{
					state.kumi3[4][1] = 2;
				}
				else if (count == 4)
				{
					state.kumi4[4][1] = 2;
				}
				else if (count == 5)
				{
					state.kumi5[4][1] = 2;
				}
				else if (count == 6)
				{
					state.kumi6[4][1] = 2;
				}
				else if (count == 7)
				{
					state.kumi7[4][1] = 2;
				}
				else if (count == 8)
				{
					state.kumi8[4][1] = 2;
				}
				else if (count == 9)
				{
					state.kumi9[4][1] = 2;
				}
				else if (count == 10)
				{
					state.kumi10[4][1] = 2;
				}
				else if (count == 11)
				{
					state.kumi11[4][1] = 2;
				}
				else if (count == 12)
				{
					state.kumi12[4][1] = 2;
				}
				copytable[4][1] = 0;
			}

			if (count == 0)
			{
				point = setvalue_single(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_single(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_single(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_single(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_single(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_single(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_single(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_single(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_single(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_single(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_single(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_single(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_single(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		/*printf("kumi:%d   point:%d\n",count,point);
		
						for(i=0;i<5;i++){
    	 					for(j=0;j<15;j++){
      						
    	 						if(count==0){
    	 							printf("%i ",state.kumi0[i][j]);
								}else if(count==1){
									printf("%i ",state.kumi1[i][j]);
								}else if(count==2){
									printf("%i ",state.kumi2[i][j]);
								}else if(count==3){
									printf("%i ",state.kumi3[i][j]);
								}else if(count==4){
									printf("%i ",state.kumi4[i][j]);
								}else if(count==5){
									printf("%i ",state.kumi5[i][j]);
								}else if(count==6){
									printf("%i ",state.kumi6[i][j]);
								}else if(count==7){
									printf("%i ",state.kumi7[i][j]);
								}else if(count==8){
									printf("%i ",state.kumi8[i][j]);
								}else if(count==9){
									printf("%i ",state.kumi9[i][j]);
								}else if(count==10){
									printf("%i ",state.kumi10[i][j]);
								}
    						}
    						printf("\n");

						}

						printf("\n");*/

		count++;
	}
	setvalue_hands();
}

int bunkatsu_noprint_a_c(int table[8][15])
{
	int i, j, k, count = 0, point = 0, totalpoint = 0;
	int kaidantable[8][15] = {{0}};
	int pairtable[8][15] = {{0}};
	int copytable[8][15] = {{0}};
	int kaidanlen = 0;
	int pairmai = 0;
	int maisuu = 0;
	int c = 0, kumisuu = 0;
	int count2 = 0;

	//階段→ペア→単体

	clearTable(state.kumi0); //追加.組を保存する配列を一試合ごとにリセット。
	clearTable(state.kumi1);
	clearTable(state.kumi2);
	clearTable(state.kumi3);
	clearTable(state.kumi4);
	clearTable(state.kumi5);
	clearTable(state.kumi6);
	clearTable(state.kumi7);
	clearTable(state.kumi8);
	clearTable(state.kumi9);
	clearTable(state.kumi10);
	clearTable(state.kumi11);
	clearTable(state.kumi12);
	/*
						for(i=0;i<14;i++){
							clearkumi_info(i);
						}
						*/
	clearkumi_info2();

	for (i = 0; i < 5; i++)
	{
		for (j = 0; j < 15; j++)
		{
			copytable[i][j] = table[i][j]; //copytableの作成
		}
	}

	while (qtyOfCards(copytable) > 0)
	{

		maisuu = 0; //リセット

		for (i = 0; i < 4; i++)
		{ //階段tableの作成
			for (j = 13; j > 0; j--)
			{
				if (copytable[i][j] == 1 || copytable[i][j] == 2)
				{
					kaidantable[i][j] = kaidantable[i][j + 1] + 1;
				}
				else
				{
					kaidantable[i][j] = 0;
				}
			}
		}

		for (i = 0; i < 4; i++)
		{ //pairtableの作成
			for (j = 1; j < 14; j++)
			{
				if (copytable[i][j] == 1 || copytable[i][j] == 2)
				{
					pairtable[i][j] = copytable[0][j] + copytable[1][j] + copytable[2][j] + copytable[3][j];
					if (copytable[0][j] == 2 || copytable[1][j] == 2 || copytable[2][j] == 2 || copytable[3][j] == 2)
					{
						pairtable[i][j] = pairtable[i][j] - 1;
					}
				}
				else
				{
					pairtable[i][j] = 0;
				}
			}
		}

		for (i = 0; i < 4; i++)
		{ //test
			for (j = 1; j < 14; j++)
			{
				if (kaidantable[i][j] >= 5)
					kaidantable[i][j] = 4;
			}
		}

		for (i = 0; i < 4; i++)
		{ //test
			for (j = 1; j < 14; j++)
			{
				if (pairtable[i][j] == 4)
					pairtable[i][j] = 3;
			}
		}

		if (qtyOfCards3(kaidantable) > 0)
		{

			for (kaidanlen = 11; kaidanlen > 2; kaidanlen--)
			{
				if (maisuu > 2)
					break;
				for (i = 0; i < 4; i++)
				{
					if (maisuu > 2)
						break;
					for (j = 1; j < 14; j++)
					{
						if (maisuu > 2)
							break;
						if (kaidantable[i][j] == kaidanlen)
						{
							for (k = 0; k < kaidanlen; k++)
							{

								if (count == 0)
								{
									state.kumi0[i][j + k] = copytable[i][j + k];
								}
								else if (count == 1)
								{
									state.kumi1[i][j + k] = copytable[i][j + k];
								}
								else if (count == 2)
								{
									state.kumi2[i][j + k] = copytable[i][j + k];
								}
								else if (count == 3)
								{
									state.kumi3[i][j + k] = copytable[i][j + k];
								}
								else if (count == 4)
								{
									state.kumi4[i][j + k] = copytable[i][j + k];
								}
								else if (count == 5)
								{
									state.kumi5[i][j + k] = copytable[i][j + k];
								}
								else if (count == 6)
								{
									state.kumi6[i][j + k] = copytable[i][j + k];
								}
								else if (count == 7)
								{
									state.kumi7[i][j + k] = copytable[i][j + k];
								}
								else if (count == 8)
								{
									state.kumi8[i][j + k] = copytable[i][j + k];
								}
								else if (count == 9)
								{
									state.kumi9[i][j + k] = copytable[i][j + k];
								}
								else if (count == 10)
								{
									state.kumi10[i][j + k] = copytable[i][j + k];
								}
								else if (count == 11)
								{
									state.kumi11[i][j + k] = copytable[i][j + k];
								}
								else if (count == 12)
								{
									state.kumi12[i][j + k] = copytable[i][j + k];
								}
								copytable[i][j + k] = 0;
							}

							if (count == 0)
							{
								maisuu = qtyOfCards(state.kumi0);
							}
							else if (count == 1)
							{
								maisuu = qtyOfCards(state.kumi1);
							}
							else if (count == 2)
							{
								maisuu = qtyOfCards(state.kumi2);
							}
							else if (count == 3)
							{
								maisuu = qtyOfCards(state.kumi3);
							}
							else if (count == 4)
							{
								maisuu = qtyOfCards(state.kumi4);
							}
							else if (count == 5)
							{
								maisuu = qtyOfCards(state.kumi5);
							}
							else if (count == 6)
							{
								maisuu = qtyOfCards(state.kumi6);
							}
							else if (count == 7)
							{
								maisuu = qtyOfCards(state.kumi7);
							}
							else if (count == 8)
							{
								maisuu = qtyOfCards(state.kumi8);
							}
							else if (count == 9)
							{
								maisuu = qtyOfCards(state.kumi9);
							}
							else if (count == 10)
							{
								maisuu = qtyOfCards(state.kumi10);
							}
							else if (count == 11)
							{
								maisuu = qtyOfCards(state.kumi11);
							}
							else if (count == 12)
							{
								maisuu = qtyOfCards(state.kumi12);
							}
						}
					}
				}
			}
			if (count == 0)
			{
				point = setvalue_kaidan(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_kaidan(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_kaidan(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_kaidan(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_kaidan(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_kaidan(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_kaidan(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_kaidan(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_kaidan(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_kaidan(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_kaidan(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_kaidan(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_kaidan(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		else if (qtyOfCards2(pairtable) > 0)
		{

			//maisuu=0;

			for (pairmai = 5; pairmai > 1; pairmai--)
			{
				if (maisuu > 1)
					break;
				for (i = 0; i < 4; i++)
				{
					if (maisuu > 1)
						break;
					for (j = 1; j < 14; j++)
					{
						if (maisuu > 1)
							break;
						if (pairtable[i][j] == pairmai)
						{

							for (k = 0; k < 4; k++)
							{
								if (copytable[k][j] == 1 || copytable[k][j] == 2)
								{

									if (count == 0)
									{
										state.kumi0[k][j] = copytable[k][j];
									}
									else if (count == 1)
									{
										state.kumi1[k][j] = copytable[k][j];
									}
									else if (count == 2)
									{
										state.kumi2[k][j] = copytable[k][j];
									}
									else if (count == 3)
									{
										state.kumi3[k][j] = copytable[k][j];
									}
									else if (count == 4)
									{
										;
										state.kumi4[k][j] = copytable[k][j];
									}
									else if (count == 5)
									{
										state.kumi5[k][j] = copytable[k][j];
									}
									else if (count == 6)
									{
										state.kumi6[k][j] = copytable[k][j];
									}
									else if (count == 7)
									{
										state.kumi7[k][j] = copytable[k][j];
									}
									else if (count == 8)
									{
										state.kumi8[k][j] = copytable[k][j];
									}
									else if (count == 9)
									{
										state.kumi9[k][j] = copytable[k][j];
									}
									else if (count == 10)
									{
										state.kumi10[k][j] = copytable[k][j];
									}
									else if (count == 11)
									{
										state.kumi11[k][j] = copytable[k][j];
									}
									else if (count == 12)
									{
										state.kumi12[k][j] = copytable[k][j];
									}

									copytable[k][j] = 0;

									count2 = count2 + 1;

									if (count2 == pairmai)
									{
										count2 = 0;
										break;
									}
								}
							}

							if (count == 0)
							{
								maisuu = qtyOfCards(state.kumi0);
							}
							else if (count == 1)
							{
								maisuu = qtyOfCards(state.kumi1);
							}
							else if (count == 2)
							{
								maisuu = qtyOfCards(state.kumi2);
							}
							else if (count == 3)
							{
								maisuu = qtyOfCards(state.kumi3);
							}
							else if (count == 4)
							{
								maisuu = qtyOfCards(state.kumi4);
							}
							else if (count == 5)
							{
								maisuu = qtyOfCards(state.kumi5);
							}
							else if (count == 6)
							{
								maisuu = qtyOfCards(state.kumi6);
							}
							else if (count == 7)
							{
								maisuu = qtyOfCards(state.kumi7);
							}
							else if (count == 8)
							{
								maisuu = qtyOfCards(state.kumi8);
							}
							else if (count == 9)
							{
								maisuu = qtyOfCards(state.kumi9);
							}
							else if (count == 10)
							{
								maisuu = qtyOfCards(state.kumi10);
							}
							else if (count == 11)
							{
								maisuu = qtyOfCards(state.kumi11);
							}
							else if (count == 12)
							{
								maisuu = qtyOfCards(state.kumi12);
							}
						}
					}
				}
			}
			if (count == 0)
			{
				point = setvalue_pair(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_pair(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_pair(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_pair(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_pair(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_pair(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_pair(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_pair(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_pair(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_pair(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_pair(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_pair(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_pair(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		else if (qtyOfCards(copytable) > 0)
		{

			for (i = 0; i < 4; i++)
			{
				if (maisuu == 1)
					break;
				for (j = 1; j < 14; j++)
				{
					if (maisuu == 1)
						break;
					if (copytable[i][j] == 1 || copytable[i][j] == 2)
					{

						if (count == 0)
						{
							state.kumi0[i][j] = copytable[i][j];
						}
						else if (count == 1)
						{
							state.kumi1[i][j] = copytable[i][j];
						}
						else if (count == 2)
						{
							state.kumi2[i][j] = copytable[i][j];
						}
						else if (count == 3)
						{
							state.kumi3[i][j] = copytable[i][j];
						}
						else if (count == 4)
						{
							state.kumi4[i][j] = copytable[i][j];
						}
						else if (count == 5)
						{
							state.kumi5[i][j] = copytable[i][j];
						}
						else if (count == 6)
						{
							state.kumi6[i][j] = copytable[i][j];
						}
						else if (count == 7)
						{
							state.kumi7[i][j] = copytable[i][j];
						}
						else if (count == 8)
						{
							state.kumi8[i][j] = copytable[i][j];
						}
						else if (count == 9)
						{
							state.kumi9[i][j] = copytable[i][j];
						}
						else if (count == 10)
						{
							state.kumi10[i][j] = copytable[i][j];
						}
						else if (count == 11)
						{
							state.kumi11[i][j] = copytable[i][j];
						}
						else if (count == 12)
						{
							state.kumi12[i][j] = copytable[i][j];
						}

						copytable[i][j] = 0;

						if (count == 0)
						{
							maisuu = qtyOfCards(state.kumi0);
						}
						else if (count == 1)
						{
							maisuu = qtyOfCards(state.kumi1);
						}
						else if (count == 2)
						{
							maisuu = qtyOfCards(state.kumi2);
						}
						else if (count == 3)
						{
							maisuu = qtyOfCards(state.kumi3);
						}
						else if (count == 4)
						{
							maisuu = qtyOfCards(state.kumi4);
						}
						else if (count == 5)
						{
							maisuu = qtyOfCards(state.kumi5);
						}
						else if (count == 6)
						{
							maisuu = qtyOfCards(state.kumi6);
						}
						else if (count == 7)
						{
							maisuu = qtyOfCards(state.kumi7);
						}
						else if (count == 8)
						{
							maisuu = qtyOfCards(state.kumi8);
						}
						else if (count == 9)
						{
							maisuu = qtyOfCards(state.kumi9);
						}
						else if (count == 10)
						{
							maisuu = qtyOfCards(state.kumi10);
						}
						else if (count == 11)
						{
							maisuu = qtyOfCards(state.kumi11);
						}
						else if (count == 12)
						{
							maisuu = qtyOfCards(state.kumi12);
						}
					}
				}
			}

			if (maisuu == 0 && joker_check(copytable) == 2)
			{
				if (count == 0)
				{
					state.kumi0[4][1] = 2;
				}
				else if (count == 1)
				{
					state.kumi1[4][1] = 2;
				}
				else if (count == 2)
				{
					state.kumi2[4][1] = 2;
				}
				else if (count == 3)
				{
					state.kumi3[4][1] = 2;
				}
				else if (count == 4)
				{
					state.kumi4[4][1] = 2;
				}
				else if (count == 5)
				{
					state.kumi5[4][1] = 2;
				}
				else if (count == 6)
				{
					state.kumi6[4][1] = 2;
				}
				else if (count == 7)
				{
					state.kumi7[4][1] = 2;
				}
				else if (count == 8)
				{
					state.kumi8[4][1] = 2;
				}
				else if (count == 9)
				{
					state.kumi9[4][1] = 2;
				}
				else if (count == 10)
				{
					state.kumi10[4][1] = 2;
				}
				else if (count == 11)
				{
					state.kumi11[4][1] = 2;
				}
				else if (count == 12)
				{
					state.kumi12[4][1] = 2;
				}
				copytable[4][1] = 0;
			}

			if (count == 0)
			{
				point = setvalue_single(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_single(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_single(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_single(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_single(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_single(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_single(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_single(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_single(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_single(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_single(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_single(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_single(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		/*printf("kumi:%d   point:%d\n",count,point);
		
						for(i=0;i<5;i++){
    	 					for(j=0;j<15;j++){
      						
    	 						if(count==0){
    	 							printf("%i ",state.kumi0[i][j]);
								}else if(count==1){
									printf("%i ",state.kumi1[i][j]);
								}else if(count==2){
									printf("%i ",state.kumi2[i][j]);
								}else if(count==3){
									printf("%i ",state.kumi3[i][j]);
								}else if(count==4){
									printf("%i ",state.kumi4[i][j]);
								}else if(count==5){
									printf("%i ",state.kumi5[i][j]);
								}else if(count==6){
									printf("%i ",state.kumi6[i][j]);
								}else if(count==7){
									printf("%i ",state.kumi7[i][j]);
								}else if(count==8){
									printf("%i ",state.kumi8[i][j]);
								}else if(count==9){
									printf("%i ",state.kumi9[i][j]);
								}else if(count==10){
									printf("%i ",state.kumi10[i][j]);
								}
    						}
    						printf("\n");

						}

						printf("\n");*/

		count++;
	}
	setvalue_hands();
}

int bunkatsu_noprint2_a_c(int table[8][15])
{
	int i, j, k, count = 0, point = 0, totalpoint = 0;
	int kaidantable[8][15] = {{0}};
	int pairtable[8][15] = {{0}};
	int copytable[8][15] = {{0}};
	int kaidanlen = 0;
	int pairmai = 0;
	int maisuu = 0;
	int c = 0, kumisuu = 0;
	int count2 = 0;

	//ペア→階段→単体

	clearTable(state.kumi0); //追加.組を保存する配列を一試合ごとにリセット。
	clearTable(state.kumi1);
	clearTable(state.kumi2);
	clearTable(state.kumi3);
	clearTable(state.kumi4);
	clearTable(state.kumi5);
	clearTable(state.kumi6);
	clearTable(state.kumi7);
	clearTable(state.kumi8);
	clearTable(state.kumi9);
	clearTable(state.kumi10);
	clearTable(state.kumi11);
	clearTable(state.kumi12);
	/*
						for(i=0;i<14;i++){
							clearkumi_info(i);
						}
						*/
	clearkumi_info2();
	for (i = 0; i < 5; i++)
	{
		for (j = 0; j < 15; j++)
		{
			copytable[i][j] = table[i][j]; //copytableの作成
		}
	}

	while (qtyOfCards(copytable) > 0)
	{

		maisuu = 0; //リセット

		for (i = 0; i < 4; i++)
		{ //階段tableの作成
			for (j = 13; j > 0; j--)
			{
				if (copytable[i][j] == 1 || copytable[i][j] == 2)
				{
					kaidantable[i][j] = kaidantable[i][j + 1] + 1;
				}
				else
				{
					kaidantable[i][j] = 0;
				}
			}
		}

		for (i = 0; i < 4; i++)
		{ //pairtableの作成
			for (j = 1; j < 14; j++)
			{
				if (copytable[i][j] == 1 || copytable[i][j] == 2)
				{
					pairtable[i][j] = copytable[0][j] + copytable[1][j] + copytable[2][j] + copytable[3][j];
					if (copytable[0][j] == 2 || copytable[1][j] == 2 || copytable[2][j] == 2 || copytable[3][j] == 2)
					{
						pairtable[i][j] = pairtable[i][j] - 1;
					}
				}
				else
				{
					pairtable[i][j] = 0;
				}
			}
		}

		for (i = 0; i < 4; i++)
		{ //test
			for (j = 1; j < 14; j++)
			{
				if (kaidantable[i][j] >= 5)
					kaidantable[i][j] = 4;
			}
		}

		for (i = 0; i < 4; i++)
		{ //test
			for (j = 1; j < 14; j++)
			{
				if (pairtable[i][j] == 4)
					pairtable[i][j] = 3;
			}
		}

		if (qtyOfCards2(pairtable) > 0)
		{

			//maisuu=0;

			for (pairmai = 5; pairmai > 1; pairmai--)
			{
				if (maisuu > 1)
					break;
				for (i = 0; i < 4; i++)
				{
					if (maisuu > 1)
						break;
					for (j = 1; j < 14; j++)
					{
						if (maisuu > 1)
							break;
						if (pairtable[i][j] == pairmai)
						{

							for (k = 0; k < 4; k++)
							{
								if (copytable[k][j] == 1 || copytable[k][j] == 2)
								{

									if (count == 0)
									{
										state.kumi0[k][j] = copytable[k][j];
									}
									else if (count == 1)
									{
										state.kumi1[k][j] = copytable[k][j];
									}
									else if (count == 2)
									{
										state.kumi2[k][j] = copytable[k][j];
									}
									else if (count == 3)
									{
										state.kumi3[k][j] = copytable[k][j];
									}
									else if (count == 4)
									{
										;
										state.kumi4[k][j] = copytable[k][j];
									}
									else if (count == 5)
									{
										state.kumi5[k][j] = copytable[k][j];
									}
									else if (count == 6)
									{
										state.kumi6[k][j] = copytable[k][j];
									}
									else if (count == 7)
									{
										state.kumi7[k][j] = copytable[k][j];
									}
									else if (count == 8)
									{
										state.kumi8[k][j] = copytable[k][j];
									}
									else if (count == 9)
									{
										state.kumi9[k][j] = copytable[k][j];
									}
									else if (count == 10)
									{
										state.kumi10[k][j] = copytable[k][j];
									}
									else if (count == 11)
									{
										state.kumi11[k][j] = copytable[k][j];
									}
									else if (count == 12)
									{
										state.kumi12[k][j] = copytable[k][j];
									}

									copytable[k][j] = 0;

									count2 = count2 + 1;

									if (count2 == pairmai)
									{
										count2 = 0;
										break;
									}
								}
							}

							if (count == 0)
							{
								maisuu = qtyOfCards(state.kumi0);
							}
							else if (count == 1)
							{
								maisuu = qtyOfCards(state.kumi1);
							}
							else if (count == 2)
							{
								maisuu = qtyOfCards(state.kumi2);
							}
							else if (count == 3)
							{
								maisuu = qtyOfCards(state.kumi3);
							}
							else if (count == 4)
							{
								maisuu = qtyOfCards(state.kumi4);
							}
							else if (count == 5)
							{
								maisuu = qtyOfCards(state.kumi5);
							}
							else if (count == 6)
							{
								maisuu = qtyOfCards(state.kumi6);
							}
							else if (count == 7)
							{
								maisuu = qtyOfCards(state.kumi7);
							}
							else if (count == 8)
							{
								maisuu = qtyOfCards(state.kumi8);
							}
							else if (count == 9)
							{
								maisuu = qtyOfCards(state.kumi9);
							}
							else if (count == 10)
							{
								maisuu = qtyOfCards(state.kumi10);
							}
							else if (count == 11)
							{
								maisuu = qtyOfCards(state.kumi11);
							}
							else if (count == 12)
							{
								maisuu = qtyOfCards(state.kumi12);
							}
						}
					}
				}
			}
			if (count == 0)
			{
				point = setvalue_pair(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_pair(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_pair(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_pair(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_pair(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_pair(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_pair(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_pair(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_pair(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_pair(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_pair(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_pair(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_pair(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		else if (qtyOfCards3(kaidantable) > 0)
		{

			for (kaidanlen = 11; kaidanlen > 2; kaidanlen--)
			{
				if (maisuu > 2)
					break;
				for (i = 0; i < 4; i++)
				{
					if (maisuu > 2)
						break;
					for (j = 1; j < 14; j++)
					{
						if (maisuu > 2)
							break;
						if (kaidantable[i][j] == kaidanlen)
						{
							for (k = 0; k < kaidanlen; k++)
							{

								if (count == 0)
								{
									state.kumi0[i][j + k] = copytable[i][j + k];
								}
								else if (count == 1)
								{
									state.kumi1[i][j + k] = copytable[i][j + k];
								}
								else if (count == 2)
								{
									state.kumi2[i][j + k] = copytable[i][j + k];
								}
								else if (count == 3)
								{
									state.kumi3[i][j + k] = copytable[i][j + k];
								}
								else if (count == 4)
								{
									state.kumi4[i][j + k] = copytable[i][j + k];
								}
								else if (count == 5)
								{
									state.kumi5[i][j + k] = copytable[i][j + k];
								}
								else if (count == 6)
								{
									state.kumi6[i][j + k] = copytable[i][j + k];
								}
								else if (count == 7)
								{
									state.kumi7[i][j + k] = copytable[i][j + k];
								}
								else if (count == 8)
								{
									state.kumi8[i][j + k] = copytable[i][j + k];
								}
								else if (count == 9)
								{
									state.kumi9[i][j + k] = copytable[i][j + k];
								}
								else if (count == 10)
								{
									state.kumi10[i][j + k] = copytable[i][j + k];
								}
								else if (count == 11)
								{
									state.kumi11[i][j + k] = copytable[i][j + k];
								}
								else if (count == 12)
								{
									state.kumi12[i][j + k] = copytable[i][j + k];
								}
								copytable[i][j + k] = 0;
							}

							if (count == 0)
							{
								maisuu = qtyOfCards(state.kumi0);
							}
							else if (count == 1)
							{
								maisuu = qtyOfCards(state.kumi1);
							}
							else if (count == 2)
							{
								maisuu = qtyOfCards(state.kumi2);
							}
							else if (count == 3)
							{
								maisuu = qtyOfCards(state.kumi3);
							}
							else if (count == 4)
							{
								maisuu = qtyOfCards(state.kumi4);
							}
							else if (count == 5)
							{
								maisuu = qtyOfCards(state.kumi5);
							}
							else if (count == 6)
							{
								maisuu = qtyOfCards(state.kumi6);
							}
							else if (count == 7)
							{
								maisuu = qtyOfCards(state.kumi7);
							}
							else if (count == 8)
							{
								maisuu = qtyOfCards(state.kumi8);
							}
							else if (count == 9)
							{
								maisuu = qtyOfCards(state.kumi9);
							}
							else if (count == 10)
							{
								maisuu = qtyOfCards(state.kumi10);
							}
							else if (count == 11)
							{
								maisuu = qtyOfCards(state.kumi11);
							}
							else if (count == 12)
							{
								maisuu = qtyOfCards(state.kumi12);
							}
						}
					}
				}
			}
			if (count == 0)
			{
				point = setvalue_kaidan(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_kaidan(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_kaidan(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_kaidan(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_kaidan(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_kaidan(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_kaidan(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_kaidan(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_kaidan(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_kaidan(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_kaidan(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_kaidan(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_kaidan(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		else if (qtyOfCards(copytable) > 0)
		{

			for (i = 0; i < 4; i++)
			{
				if (maisuu == 1)
					break;
				for (j = 1; j < 14; j++)
				{
					if (maisuu == 1)
						break;
					if (copytable[i][j] == 1 || copytable[i][j] == 2)
					{

						if (count == 0)
						{
							state.kumi0[i][j] = copytable[i][j];
						}
						else if (count == 1)
						{
							state.kumi1[i][j] = copytable[i][j];
						}
						else if (count == 2)
						{
							state.kumi2[i][j] = copytable[i][j];
						}
						else if (count == 3)
						{
							state.kumi3[i][j] = copytable[i][j];
						}
						else if (count == 4)
						{
							state.kumi4[i][j] = copytable[i][j];
						}
						else if (count == 5)
						{
							state.kumi5[i][j] = copytable[i][j];
						}
						else if (count == 6)
						{
							state.kumi6[i][j] = copytable[i][j];
						}
						else if (count == 7)
						{
							state.kumi7[i][j] = copytable[i][j];
						}
						else if (count == 8)
						{
							state.kumi8[i][j] = copytable[i][j];
						}
						else if (count == 9)
						{
							state.kumi9[i][j] = copytable[i][j];
						}
						else if (count == 10)
						{
							state.kumi10[i][j] = copytable[i][j];
						}
						else if (count == 11)
						{
							state.kumi11[i][j] = copytable[i][j];
						}
						else if (count == 12)
						{
							state.kumi12[i][j] = copytable[i][j];
						}

						copytable[i][j] = 0;

						if (count == 0)
						{
							maisuu = qtyOfCards(state.kumi0);
						}
						else if (count == 1)
						{
							maisuu = qtyOfCards(state.kumi1);
						}
						else if (count == 2)
						{
							maisuu = qtyOfCards(state.kumi2);
						}
						else if (count == 3)
						{
							maisuu = qtyOfCards(state.kumi3);
						}
						else if (count == 4)
						{
							maisuu = qtyOfCards(state.kumi4);
						}
						else if (count == 5)
						{
							maisuu = qtyOfCards(state.kumi5);
						}
						else if (count == 6)
						{
							maisuu = qtyOfCards(state.kumi6);
						}
						else if (count == 7)
						{
							maisuu = qtyOfCards(state.kumi7);
						}
						else if (count == 8)
						{
							maisuu = qtyOfCards(state.kumi8);
						}
						else if (count == 9)
						{
							maisuu = qtyOfCards(state.kumi9);
						}
						else if (count == 10)
						{
							maisuu = qtyOfCards(state.kumi10);
						}
						else if (count == 11)
						{
							maisuu = qtyOfCards(state.kumi11);
						}
						else if (count == 12)
						{
							maisuu = qtyOfCards(state.kumi12);
						}
					}
				}
			}

			if (maisuu == 0 && joker_check(copytable) == 2)
			{
				if (count == 0)
				{
					state.kumi0[4][1] = 2;
				}
				else if (count == 1)
				{
					state.kumi1[4][1] = 2;
				}
				else if (count == 2)
				{
					state.kumi2[4][1] = 2;
				}
				else if (count == 3)
				{
					state.kumi3[4][1] = 2;
				}
				else if (count == 4)
				{
					state.kumi4[4][1] = 2;
				}
				else if (count == 5)
				{
					state.kumi5[4][1] = 2;
				}
				else if (count == 6)
				{
					state.kumi6[4][1] = 2;
				}
				else if (count == 7)
				{
					state.kumi7[4][1] = 2;
				}
				else if (count == 8)
				{
					state.kumi8[4][1] = 2;
				}
				else if (count == 9)
				{
					state.kumi9[4][1] = 2;
				}
				else if (count == 10)
				{
					state.kumi10[4][1] = 2;
				}
				else if (count == 11)
				{
					state.kumi11[4][1] = 2;
				}
				else if (count == 12)
				{
					state.kumi12[4][1] = 2;
				}
				copytable[4][1] = 0;
			}

			if (count == 0)
			{
				point = setvalue_single(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_single(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_single(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_single(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_single(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_single(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_single(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_single(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_single(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_single(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_single(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_single(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_single(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		/*printf("kumi:%d   point:%d\n",count,point);
		
						for(i=0;i<5;i++){
    	 					for(j=0;j<15;j++){
      						
    	 						if(count==0){
    	 							printf("%i ",state.kumi0[i][j]);
								}else if(count==1){
									printf("%i ",state.kumi1[i][j]);
								}else if(count==2){
									printf("%i ",state.kumi2[i][j]);
								}else if(count==3){
									printf("%i ",state.kumi3[i][j]);
								}else if(count==4){
									printf("%i ",state.kumi4[i][j]);
								}else if(count==5){
									printf("%i ",state.kumi5[i][j]);
								}else if(count==6){
									printf("%i ",state.kumi6[i][j]);
								}else if(count==7){
									printf("%i ",state.kumi7[i][j]);
								}else if(count==8){
									printf("%i ",state.kumi8[i][j]);
								}else if(count==9){
									printf("%i ",state.kumi9[i][j]);
								}else if(count==10){
									printf("%i ",state.kumi10[i][j]);
								}
    						}
    						printf("\n");

						}

						printf("\n");*/

		count++;
	}
	setvalue_hands();
}

int bunkatsu_noprint_rev(int table[8][15])
{
	int i, j, k, count = 0, point = 0, totalpoint = 0;
	int kaidantable[8][15] = {{0}};
	int pairtable[8][15] = {{0}};
	int copytable[8][15] = {{0}};
	int kaidanlen = 0;
	int pairmai = 0;
	int maisuu = 0;
	int c = 0, kumisuu = 0;
	int count2 = 0;

	//階段→ペア→単体

	clearTable(state.kumi0); //追加.組を保存する配列を一試合ごとにリセット。
	clearTable(state.kumi1);
	clearTable(state.kumi2);
	clearTable(state.kumi3);
	clearTable(state.kumi4);
	clearTable(state.kumi5);
	clearTable(state.kumi6);
	clearTable(state.kumi7);
	clearTable(state.kumi8);
	clearTable(state.kumi9);
	clearTable(state.kumi10);
	clearTable(state.kumi11);
	clearTable(state.kumi12);
	/*
						for(i=0;i<14;i++){
							clearkumi_info(i);
						}
						*/
	clearkumi_info2();
	for (i = 0; i < 5; i++)
	{
		for (j = 0; j < 15; j++)
		{
			copytable[i][j] = table[i][j]; //copytableの作成
		}
	}

	while (qtyOfCards(copytable) > 0)
	{

		maisuu = 0; //リセット

		for (i = 0; i < 4; i++)
		{ //階段tableの作成
			for (j = 13; j > 0 + 1; j--)
			{
				if (copytable[i][j] == 1 || copytable[i][j] == 2)
				{
					kaidantable[i][j] = kaidantable[i][j + 1] + 1;
				}
				else
				{
					kaidantable[i][j] = 0;
				}
			}
		}

		for (i = 0; i < 4; i++)
		{ //pairtableの作成
			for (j = 1 + 1; j < 14; j++)
			{
				if (copytable[i][j] == 1 || copytable[i][j] == 2)
				{
					pairtable[i][j] = copytable[0][j] + copytable[1][j] + copytable[2][j] + copytable[3][j];
					if (copytable[0][j] == 2 || copytable[1][j] == 2 || copytable[2][j] == 2 || copytable[3][j] == 2)
					{
						pairtable[i][j] = pairtable[i][j] - 1;
					}
				}
				else
				{
					pairtable[i][j] = 0;
				}
			}
		}
		/*
						for(i=0;i<4;i++){//test
							for(j=1;j<14;j++){
								if(kaidantable[i][j]>=5)kaidantable[i][j]=4;
							}
						}
		*/
		/*
						for(i=0;i<4;i++){//test
							for(j=1;j<14;j++){
								if(pairtable[i][j]==4)pairtable[i][j]=2;
							}
						}
		*/

		if (qtyOfCards3(kaidantable) > 0)
		{

			for (kaidanlen = 11; kaidanlen > 2; kaidanlen--)
			{
				if (maisuu > 2)
					break;
				for (i = 0; i < 4; i++)
				{
					if (maisuu > 2)
						break;
					for (j = 1; j < 14; j++)
					{
						if (maisuu > 2)
							break;
						if (kaidantable[i][j] == kaidanlen)
						{
							for (k = 0; k < kaidanlen; k++)
							{

								if (count == 0)
								{
									state.kumi0[i][j + k] = copytable[i][j + k];
								}
								else if (count == 1)
								{
									state.kumi1[i][j + k] = copytable[i][j + k];
								}
								else if (count == 2)
								{
									state.kumi2[i][j + k] = copytable[i][j + k];
								}
								else if (count == 3)
								{
									state.kumi3[i][j + k] = copytable[i][j + k];
								}
								else if (count == 4)
								{
									state.kumi4[i][j + k] = copytable[i][j + k];
								}
								else if (count == 5)
								{
									state.kumi5[i][j + k] = copytable[i][j + k];
								}
								else if (count == 6)
								{
									state.kumi6[i][j + k] = copytable[i][j + k];
								}
								else if (count == 7)
								{
									state.kumi7[i][j + k] = copytable[i][j + k];
								}
								else if (count == 8)
								{
									state.kumi8[i][j + k] = copytable[i][j + k];
								}
								else if (count == 9)
								{
									state.kumi9[i][j + k] = copytable[i][j + k];
								}
								else if (count == 10)
								{
									state.kumi10[i][j + k] = copytable[i][j + k];
								}
								else if (count == 11)
								{
									state.kumi11[i][j + k] = copytable[i][j + k];
								}
								else if (count == 12)
								{
									state.kumi12[i][j + k] = copytable[i][j + k];
								}
								copytable[i][j + k] = 0;
							}

							if (count == 0)
							{
								maisuu = qtyOfCards(state.kumi0);
							}
							else if (count == 1)
							{
								maisuu = qtyOfCards(state.kumi1);
							}
							else if (count == 2)
							{
								maisuu = qtyOfCards(state.kumi2);
							}
							else if (count == 3)
							{
								maisuu = qtyOfCards(state.kumi3);
							}
							else if (count == 4)
							{
								maisuu = qtyOfCards(state.kumi4);
							}
							else if (count == 5)
							{
								maisuu = qtyOfCards(state.kumi5);
							}
							else if (count == 6)
							{
								maisuu = qtyOfCards(state.kumi6);
							}
							else if (count == 7)
							{
								maisuu = qtyOfCards(state.kumi7);
							}
							else if (count == 8)
							{
								maisuu = qtyOfCards(state.kumi8);
							}
							else if (count == 9)
							{
								maisuu = qtyOfCards(state.kumi9);
							}
							else if (count == 10)
							{
								maisuu = qtyOfCards(state.kumi10);
							}
							else if (count == 11)
							{
								maisuu = qtyOfCards(state.kumi11);
							}
							else if (count == 12)
							{
								maisuu = qtyOfCards(state.kumi12);
							}
						}
					}
				}
			}
			if (count == 0)
			{
				point = setvalue_kaidan(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_kaidan(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_kaidan(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_kaidan(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_kaidan(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_kaidan(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_kaidan(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_kaidan(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_kaidan(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_kaidan(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_kaidan(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_kaidan(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_kaidan(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		else if (qtyOfCards2(pairtable) > 0)
		{

			//maisuu=0;

			for (pairmai = 5; pairmai > 1; pairmai--)
			{
				if (maisuu > 1)
					break;
				for (i = 0; i < 4; i++)
				{
					if (maisuu > 1)
						break;
					for (j = 1; j < 14; j++)
					{
						if (maisuu > 1)
							break;
						if (pairtable[i][j] == pairmai)
						{

							for (k = 0; k < 4; k++)
							{
								if (copytable[k][j] == 1 || copytable[k][j] == 2)
								{

									if (count == 0)
									{
										state.kumi0[k][j] = copytable[k][j];
									}
									else if (count == 1)
									{
										state.kumi1[k][j] = copytable[k][j];
									}
									else if (count == 2)
									{
										state.kumi2[k][j] = copytable[k][j];
									}
									else if (count == 3)
									{
										state.kumi3[k][j] = copytable[k][j];
									}
									else if (count == 4)
									{
										;
										state.kumi4[k][j] = copytable[k][j];
									}
									else if (count == 5)
									{
										state.kumi5[k][j] = copytable[k][j];
									}
									else if (count == 6)
									{
										state.kumi6[k][j] = copytable[k][j];
									}
									else if (count == 7)
									{
										state.kumi7[k][j] = copytable[k][j];
									}
									else if (count == 8)
									{
										state.kumi8[k][j] = copytable[k][j];
									}
									else if (count == 9)
									{
										state.kumi9[k][j] = copytable[k][j];
									}
									else if (count == 10)
									{
										state.kumi10[k][j] = copytable[k][j];
									}
									else if (count == 11)
									{
										state.kumi11[k][j] = copytable[k][j];
									}
									else if (count == 12)
									{
										state.kumi12[k][j] = copytable[k][j];
									}

									copytable[k][j] = 0;

									count2 = count2 + 1;

									if (count2 == pairmai)
									{
										count2 = 0;
										break;
									}
								}
							}

							if (count == 0)
							{
								maisuu = qtyOfCards(state.kumi0);
							}
							else if (count == 1)
							{
								maisuu = qtyOfCards(state.kumi1);
							}
							else if (count == 2)
							{
								maisuu = qtyOfCards(state.kumi2);
							}
							else if (count == 3)
							{
								maisuu = qtyOfCards(state.kumi3);
							}
							else if (count == 4)
							{
								maisuu = qtyOfCards(state.kumi4);
							}
							else if (count == 5)
							{
								maisuu = qtyOfCards(state.kumi5);
							}
							else if (count == 6)
							{
								maisuu = qtyOfCards(state.kumi6);
							}
							else if (count == 7)
							{
								maisuu = qtyOfCards(state.kumi7);
							}
							else if (count == 8)
							{
								maisuu = qtyOfCards(state.kumi8);
							}
							else if (count == 9)
							{
								maisuu = qtyOfCards(state.kumi9);
							}
							else if (count == 10)
							{
								maisuu = qtyOfCards(state.kumi10);
							}
							else if (count == 11)
							{
								maisuu = qtyOfCards(state.kumi11);
							}
							else if (count == 12)
							{
								maisuu = qtyOfCards(state.kumi12);
							}
						}
					}
				}
			}
			if (count == 0)
			{
				point = setvalue_pair(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_pair(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_pair(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_pair(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_pair(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_pair(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_pair(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_pair(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_pair(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_pair(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_pair(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_pair(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_pair(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		else if (qtyOfCards(copytable) > 0)
		{

			for (i = 0; i < 4; i++)
			{
				if (maisuu == 1)
					break;
				for (j = 1; j < 14; j++)
				{
					if (maisuu == 1)
						break;
					if (copytable[i][j] == 1 || copytable[i][j] == 2)
					{

						if (count == 0)
						{
							state.kumi0[i][j] = copytable[i][j];
						}
						else if (count == 1)
						{
							state.kumi1[i][j] = copytable[i][j];
						}
						else if (count == 2)
						{
							state.kumi2[i][j] = copytable[i][j];
						}
						else if (count == 3)
						{
							state.kumi3[i][j] = copytable[i][j];
						}
						else if (count == 4)
						{
							state.kumi4[i][j] = copytable[i][j];
						}
						else if (count == 5)
						{
							state.kumi5[i][j] = copytable[i][j];
						}
						else if (count == 6)
						{
							state.kumi6[i][j] = copytable[i][j];
						}
						else if (count == 7)
						{
							state.kumi7[i][j] = copytable[i][j];
						}
						else if (count == 8)
						{
							state.kumi8[i][j] = copytable[i][j];
						}
						else if (count == 9)
						{
							state.kumi9[i][j] = copytable[i][j];
						}
						else if (count == 10)
						{
							state.kumi10[i][j] = copytable[i][j];
						}
						else if (count == 11)
						{
							state.kumi11[i][j] = copytable[i][j];
						}
						else if (count == 12)
						{
							state.kumi12[i][j] = copytable[i][j];
						}

						copytable[i][j] = 0;

						if (count == 0)
						{
							maisuu = qtyOfCards(state.kumi0);
						}
						else if (count == 1)
						{
							maisuu = qtyOfCards(state.kumi1);
						}
						else if (count == 2)
						{
							maisuu = qtyOfCards(state.kumi2);
						}
						else if (count == 3)
						{
							maisuu = qtyOfCards(state.kumi3);
						}
						else if (count == 4)
						{
							maisuu = qtyOfCards(state.kumi4);
						}
						else if (count == 5)
						{
							maisuu = qtyOfCards(state.kumi5);
						}
						else if (count == 6)
						{
							maisuu = qtyOfCards(state.kumi6);
						}
						else if (count == 7)
						{
							maisuu = qtyOfCards(state.kumi7);
						}
						else if (count == 8)
						{
							maisuu = qtyOfCards(state.kumi8);
						}
						else if (count == 9)
						{
							maisuu = qtyOfCards(state.kumi9);
						}
						else if (count == 10)
						{
							maisuu = qtyOfCards(state.kumi10);
						}
						else if (count == 11)
						{
							maisuu = qtyOfCards(state.kumi11);
						}
						else if (count == 12)
						{
							maisuu = qtyOfCards(state.kumi12);
						}
					}
				}
			}

			if (maisuu == 0 && joker_check(copytable) == 2)
			{
				if (count == 0)
				{
					state.kumi0[4][1] = 2;
				}
				else if (count == 1)
				{
					state.kumi1[4][1] = 2;
				}
				else if (count == 2)
				{
					state.kumi2[4][1] = 2;
				}
				else if (count == 3)
				{
					state.kumi3[4][1] = 2;
				}
				else if (count == 4)
				{
					state.kumi4[4][1] = 2;
				}
				else if (count == 5)
				{
					state.kumi5[4][1] = 2;
				}
				else if (count == 6)
				{
					state.kumi6[4][1] = 2;
				}
				else if (count == 7)
				{
					state.kumi7[4][1] = 2;
				}
				else if (count == 8)
				{
					state.kumi8[4][1] = 2;
				}
				else if (count == 9)
				{
					state.kumi9[4][1] = 2;
				}
				else if (count == 10)
				{
					state.kumi10[4][1] = 2;
				}
				else if (count == 11)
				{
					state.kumi11[4][1] = 2;
				}
				else if (count == 12)
				{
					state.kumi12[4][1] = 2;
				}
				copytable[4][1] = 0;
			}

			if (count == 0)
			{
				point = setvalue_single(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_single(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_single(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_single(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_single(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_single(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_single(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_single(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_single(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_single(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_single(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_single(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_single(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		/*printf("kumi:%d   point:%d\n",count,point);
		
						for(i=0;i<5;i++){
    	 					for(j=0;j<15;j++){
      						
    	 						if(count==0){
    	 							printf("%i ",state.kumi0[i][j]);
								}else if(count==1){
									printf("%i ",state.kumi1[i][j]);
								}else if(count==2){
									printf("%i ",state.kumi2[i][j]);
								}else if(count==3){
									printf("%i ",state.kumi3[i][j]);
								}else if(count==4){
									printf("%i ",state.kumi4[i][j]);
								}else if(count==5){
									printf("%i ",state.kumi5[i][j]);
								}else if(count==6){
									printf("%i ",state.kumi6[i][j]);
								}else if(count==7){
									printf("%i ",state.kumi7[i][j]);
								}else if(count==8){
									printf("%i ",state.kumi8[i][j]);
								}else if(count==9){
									printf("%i ",state.kumi9[i][j]);
								}else if(count==10){
									printf("%i ",state.kumi10[i][j]);
								}
    						}
    						printf("\n");

						}

						printf("\n");*/

		count++;
	}
	setvalue_hands();
}

int bunkatsu_noprint2_rev(int table[8][15])
{
	int i, j, k, count = 0, point = 0, totalpoint = 0;
	int kaidantable[8][15] = {{0}};
	int pairtable[8][15] = {{0}};
	int copytable[8][15] = {{0}};
	int kaidanlen = 0;
	int pairmai = 0;
	int maisuu = 0;
	int c = 0, kumisuu = 0;
	int count2 = 0;

	//ペア→階段→単体

	clearTable(state.kumi0); //追加.組を保存する配列を一試合ごとにリセット。
	clearTable(state.kumi1);
	clearTable(state.kumi2);
	clearTable(state.kumi3);
	clearTable(state.kumi4);
	clearTable(state.kumi5);
	clearTable(state.kumi6);
	clearTable(state.kumi7);
	clearTable(state.kumi8);
	clearTable(state.kumi9);
	clearTable(state.kumi10);
	clearTable(state.kumi11);
	clearTable(state.kumi12);
	/*
						for(i=0;i<14;i++){
							clearkumi_info(i);
						}
						*/
	clearkumi_info2();
	for (i = 0; i < 5; i++)
	{
		for (j = 0; j < 15; j++)
		{
			copytable[i][j] = table[i][j]; //copytableの作成
		}
	}

	while (qtyOfCards(copytable) > 0)
	{

		maisuu = 0; //リセット

		for (i = 0; i < 4; i++)
		{ //階段tableの作成
			for (j = 13; j > 0 + 1; j--)
			{
				if (copytable[i][j] == 1 || copytable[i][j] == 2)
				{
					kaidantable[i][j] = kaidantable[i][j + 1] + 1;
				}
				else
				{
					kaidantable[i][j] = 0;
				}
			}
		}

		for (i = 0; i < 4; i++)
		{ //pairtableの作成
			for (j = 1 + 1; j < 14; j++)
			{
				if (copytable[i][j] == 1 || copytable[i][j] == 2)
				{
					pairtable[i][j] = copytable[0][j] + copytable[1][j] + copytable[2][j] + copytable[3][j];
					if (copytable[0][j] == 2 || copytable[1][j] == 2 || copytable[2][j] == 2 || copytable[3][j] == 2)
					{
						pairtable[i][j] = pairtable[i][j] - 1;
					}
				}
				else
				{
					pairtable[i][j] = 0;
				}
			}
		}
		/*
						for(i=0;i<4;i++){//test
							for(j=1;j<14;j++){
								if(kaidantable[i][j]>=5)kaidantable[i][j]=4;
							}
						}
		*/
		/*
						for(i=0;i<4;i++){//test
							for(j=1;j<14;j++){
								if(pairtable[i][j]==4)pairtable[i][j]=2;
							}
						}
		*/

		if (qtyOfCards2(pairtable) > 0)
		{

			//maisuu=0;

			for (pairmai = 5; pairmai > 1; pairmai--)
			{
				if (maisuu > 1)
					break;
				for (i = 0; i < 4; i++)
				{
					if (maisuu > 1)
						break;
					for (j = 1; j < 14; j++)
					{
						if (maisuu > 1)
							break;
						if (pairtable[i][j] == pairmai)
						{

							for (k = 0; k < 4; k++)
							{
								if (copytable[k][j] == 1 || copytable[k][j] == 2)
								{

									if (count == 0)
									{
										state.kumi0[k][j] = copytable[k][j];
									}
									else if (count == 1)
									{
										state.kumi1[k][j] = copytable[k][j];
									}
									else if (count == 2)
									{
										state.kumi2[k][j] = copytable[k][j];
									}
									else if (count == 3)
									{
										state.kumi3[k][j] = copytable[k][j];
									}
									else if (count == 4)
									{
										;
										state.kumi4[k][j] = copytable[k][j];
									}
									else if (count == 5)
									{
										state.kumi5[k][j] = copytable[k][j];
									}
									else if (count == 6)
									{
										state.kumi6[k][j] = copytable[k][j];
									}
									else if (count == 7)
									{
										state.kumi7[k][j] = copytable[k][j];
									}
									else if (count == 8)
									{
										state.kumi8[k][j] = copytable[k][j];
									}
									else if (count == 9)
									{
										state.kumi9[k][j] = copytable[k][j];
									}
									else if (count == 10)
									{
										state.kumi10[k][j] = copytable[k][j];
									}
									else if (count == 11)
									{
										state.kumi11[k][j] = copytable[k][j];
									}
									else if (count == 12)
									{
										state.kumi12[k][j] = copytable[k][j];
									}

									copytable[k][j] = 0;

									count2 = count2 + 1;

									if (count2 == pairmai)
									{
										count2 = 0;
										break;
									}
								}
							}

							if (count == 0)
							{
								maisuu = qtyOfCards(state.kumi0);
							}
							else if (count == 1)
							{
								maisuu = qtyOfCards(state.kumi1);
							}
							else if (count == 2)
							{
								maisuu = qtyOfCards(state.kumi2);
							}
							else if (count == 3)
							{
								maisuu = qtyOfCards(state.kumi3);
							}
							else if (count == 4)
							{
								maisuu = qtyOfCards(state.kumi4);
							}
							else if (count == 5)
							{
								maisuu = qtyOfCards(state.kumi5);
							}
							else if (count == 6)
							{
								maisuu = qtyOfCards(state.kumi6);
							}
							else if (count == 7)
							{
								maisuu = qtyOfCards(state.kumi7);
							}
							else if (count == 8)
							{
								maisuu = qtyOfCards(state.kumi8);
							}
							else if (count == 9)
							{
								maisuu = qtyOfCards(state.kumi9);
							}
							else if (count == 10)
							{
								maisuu = qtyOfCards(state.kumi10);
							}
							else if (count == 11)
							{
								maisuu = qtyOfCards(state.kumi11);
							}
							else if (count == 12)
							{
								maisuu = qtyOfCards(state.kumi12);
							}
						}
					}
				}
			}
			if (count == 0)
			{
				point = setvalue_pair(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_pair(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_pair(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_pair(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_pair(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_pair(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_pair(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_pair(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_pair(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_pair(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_pair(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_pair(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_pair(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		else if (qtyOfCards3(kaidantable) > 0)
		{

			for (kaidanlen = 11; kaidanlen > 2; kaidanlen--)
			{
				if (maisuu > 2)
					break;
				for (i = 0; i < 4; i++)
				{
					if (maisuu > 2)
						break;
					for (j = 1; j < 14; j++)
					{
						if (maisuu > 2)
							break;
						if (kaidantable[i][j] == kaidanlen)
						{
							for (k = 0; k < kaidanlen; k++)
							{

								if (count == 0)
								{
									state.kumi0[i][j + k] = copytable[i][j + k];
								}
								else if (count == 1)
								{
									state.kumi1[i][j + k] = copytable[i][j + k];
								}
								else if (count == 2)
								{
									state.kumi2[i][j + k] = copytable[i][j + k];
								}
								else if (count == 3)
								{
									state.kumi3[i][j + k] = copytable[i][j + k];
								}
								else if (count == 4)
								{
									state.kumi4[i][j + k] = copytable[i][j + k];
								}
								else if (count == 5)
								{
									state.kumi5[i][j + k] = copytable[i][j + k];
								}
								else if (count == 6)
								{
									state.kumi6[i][j + k] = copytable[i][j + k];
								}
								else if (count == 7)
								{
									state.kumi7[i][j + k] = copytable[i][j + k];
								}
								else if (count == 8)
								{
									state.kumi8[i][j + k] = copytable[i][j + k];
								}
								else if (count == 9)
								{
									state.kumi9[i][j + k] = copytable[i][j + k];
								}
								else if (count == 10)
								{
									state.kumi10[i][j + k] = copytable[i][j + k];
								}
								else if (count == 11)
								{
									state.kumi11[i][j + k] = copytable[i][j + k];
								}
								else if (count == 12)
								{
									state.kumi12[i][j + k] = copytable[i][j + k];
								}
								copytable[i][j + k] = 0;
							}

							if (count == 0)
							{
								maisuu = qtyOfCards(state.kumi0);
							}
							else if (count == 1)
							{
								maisuu = qtyOfCards(state.kumi1);
							}
							else if (count == 2)
							{
								maisuu = qtyOfCards(state.kumi2);
							}
							else if (count == 3)
							{
								maisuu = qtyOfCards(state.kumi3);
							}
							else if (count == 4)
							{
								maisuu = qtyOfCards(state.kumi4);
							}
							else if (count == 5)
							{
								maisuu = qtyOfCards(state.kumi5);
							}
							else if (count == 6)
							{
								maisuu = qtyOfCards(state.kumi6);
							}
							else if (count == 7)
							{
								maisuu = qtyOfCards(state.kumi7);
							}
							else if (count == 8)
							{
								maisuu = qtyOfCards(state.kumi8);
							}
							else if (count == 9)
							{
								maisuu = qtyOfCards(state.kumi9);
							}
							else if (count == 10)
							{
								maisuu = qtyOfCards(state.kumi10);
							}
							else if (count == 11)
							{
								maisuu = qtyOfCards(state.kumi11);
							}
							else if (count == 12)
							{
								maisuu = qtyOfCards(state.kumi12);
							}
						}
					}
				}
			}
			if (count == 0)
			{
				point = setvalue_kaidan(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_kaidan(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_kaidan(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_kaidan(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_kaidan(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_kaidan(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_kaidan(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_kaidan(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_kaidan(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_kaidan(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_kaidan(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_kaidan(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_kaidan(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		else if (qtyOfCards(copytable) > 0)
		{

			for (i = 0; i < 4; i++)
			{
				if (maisuu == 1)
					break;
				for (j = 1; j < 14; j++)
				{
					if (maisuu == 1)
						break;
					if (copytable[i][j] == 1 || copytable[i][j] == 2)
					{

						if (count == 0)
						{
							state.kumi0[i][j] = copytable[i][j];
						}
						else if (count == 1)
						{
							state.kumi1[i][j] = copytable[i][j];
						}
						else if (count == 2)
						{
							state.kumi2[i][j] = copytable[i][j];
						}
						else if (count == 3)
						{
							state.kumi3[i][j] = copytable[i][j];
						}
						else if (count == 4)
						{
							state.kumi4[i][j] = copytable[i][j];
						}
						else if (count == 5)
						{
							state.kumi5[i][j] = copytable[i][j];
						}
						else if (count == 6)
						{
							state.kumi6[i][j] = copytable[i][j];
						}
						else if (count == 7)
						{
							state.kumi7[i][j] = copytable[i][j];
						}
						else if (count == 8)
						{
							state.kumi8[i][j] = copytable[i][j];
						}
						else if (count == 9)
						{
							state.kumi9[i][j] = copytable[i][j];
						}
						else if (count == 10)
						{
							state.kumi10[i][j] = copytable[i][j];
						}
						else if (count == 11)
						{
							state.kumi11[i][j] = copytable[i][j];
						}
						else if (count == 12)
						{
							state.kumi12[i][j] = copytable[i][j];
						}

						copytable[i][j] = 0;

						if (count == 0)
						{
							maisuu = qtyOfCards(state.kumi0);
						}
						else if (count == 1)
						{
							maisuu = qtyOfCards(state.kumi1);
						}
						else if (count == 2)
						{
							maisuu = qtyOfCards(state.kumi2);
						}
						else if (count == 3)
						{
							maisuu = qtyOfCards(state.kumi3);
						}
						else if (count == 4)
						{
							maisuu = qtyOfCards(state.kumi4);
						}
						else if (count == 5)
						{
							maisuu = qtyOfCards(state.kumi5);
						}
						else if (count == 6)
						{
							maisuu = qtyOfCards(state.kumi6);
						}
						else if (count == 7)
						{
							maisuu = qtyOfCards(state.kumi7);
						}
						else if (count == 8)
						{
							maisuu = qtyOfCards(state.kumi8);
						}
						else if (count == 9)
						{
							maisuu = qtyOfCards(state.kumi9);
						}
						else if (count == 10)
						{
							maisuu = qtyOfCards(state.kumi10);
						}
						else if (count == 11)
						{
							maisuu = qtyOfCards(state.kumi11);
						}
						else if (count == 12)
						{
							maisuu = qtyOfCards(state.kumi12);
						}
					}
				}
			}

			if (maisuu == 0 && joker_check(copytable) == 2)
			{
				if (count == 0)
				{
					state.kumi0[4][1] = 2;
				}
				else if (count == 1)
				{
					state.kumi1[4][1] = 2;
				}
				else if (count == 2)
				{
					state.kumi2[4][1] = 2;
				}
				else if (count == 3)
				{
					state.kumi3[4][1] = 2;
				}
				else if (count == 4)
				{
					state.kumi4[4][1] = 2;
				}
				else if (count == 5)
				{
					state.kumi5[4][1] = 2;
				}
				else if (count == 6)
				{
					state.kumi6[4][1] = 2;
				}
				else if (count == 7)
				{
					state.kumi7[4][1] = 2;
				}
				else if (count == 8)
				{
					state.kumi8[4][1] = 2;
				}
				else if (count == 9)
				{
					state.kumi9[4][1] = 2;
				}
				else if (count == 10)
				{
					state.kumi10[4][1] = 2;
				}
				else if (count == 11)
				{
					state.kumi11[4][1] = 2;
				}
				else if (count == 12)
				{
					state.kumi12[4][1] = 2;
				}
				copytable[4][1] = 0;
			}

			if (count == 0)
			{
				point = setvalue_single(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_single(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_single(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_single(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_single(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_single(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_single(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_single(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_single(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_single(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_single(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_single(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_single(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		/*printf("kumi:%d   point:%d\n",count,point);
		
						for(i=0;i<5;i++){
    	 					for(j=0;j<15;j++){
      						
    	 						if(count==0){
    	 							printf("%i ",state.kumi0[i][j]);
								}else if(count==1){
									printf("%i ",state.kumi1[i][j]);
								}else if(count==2){
									printf("%i ",state.kumi2[i][j]);
								}else if(count==3){
									printf("%i ",state.kumi3[i][j]);
								}else if(count==4){
									printf("%i ",state.kumi4[i][j]);
								}else if(count==5){
									printf("%i ",state.kumi5[i][j]);
								}else if(count==6){
									printf("%i ",state.kumi6[i][j]);
								}else if(count==7){
									printf("%i ",state.kumi7[i][j]);
								}else if(count==8){
									printf("%i ",state.kumi8[i][j]);
								}else if(count==9){
									printf("%i ",state.kumi9[i][j]);
								}else if(count==10){
									printf("%i ",state.kumi10[i][j]);
								}
    						}
    						printf("\n");

						}

						printf("\n");*/

		count++;
	}
	setvalue_hands();
}

int bunkatsu_noprint_a_rev(int table[8][15])
{
	int i, j, k, count = 0, point = 0, totalpoint = 0;
	int kaidantable[8][15] = {{0}};
	int pairtable[8][15] = {{0}};
	int copytable[8][15] = {{0}};
	int kaidanlen = 0;
	int pairmai = 0;
	int maisuu = 0;
	int c = 0, kumisuu = 0;
	int count2 = 0;

	//階段→ペア→単体

	clearTable(state.kumi0); //追加.組を保存する配列を一試合ごとにリセット。
	clearTable(state.kumi1);
	clearTable(state.kumi2);
	clearTable(state.kumi3);
	clearTable(state.kumi4);
	clearTable(state.kumi5);
	clearTable(state.kumi6);
	clearTable(state.kumi7);
	clearTable(state.kumi8);
	clearTable(state.kumi9);
	clearTable(state.kumi10);
	clearTable(state.kumi11);
	clearTable(state.kumi12);
	/*
						for(i=0;i<14;i++){
							clearkumi_info(i);
						}
						*/
	clearkumi_info2();
	for (i = 0; i < 5; i++)
	{
		for (j = 0; j < 15; j++)
		{
			copytable[i][j] = table[i][j]; //copytableの作成
		}
	}

	while (qtyOfCards(copytable) > 0)
	{

		maisuu = 0; //リセット

		for (i = 0; i < 4; i++)
		{ //階段tableの作成
			for (j = 13; j > 0 + 1; j--)
			{
				if (copytable[i][j] == 1 || copytable[i][j] == 2)
				{
					kaidantable[i][j] = kaidantable[i][j + 1] + 1;
				}
				else
				{
					kaidantable[i][j] = 0;
				}
			}
		}

		for (i = 0; i < 4; i++)
		{ //pairtableの作成
			for (j = 1 + 1; j < 14; j++)
			{
				if (copytable[i][j] == 1 || copytable[i][j] == 2)
				{
					pairtable[i][j] = copytable[0][j] + copytable[1][j] + copytable[2][j] + copytable[3][j];
					if (copytable[0][j] == 2 || copytable[1][j] == 2 || copytable[2][j] == 2 || copytable[3][j] == 2)
					{
						pairtable[i][j] = pairtable[i][j] - 1;
					}
				}
				else
				{
					pairtable[i][j] = 0;
				}
			}
		}

		for (i = 0; i < 4; i++)
		{ //test
			for (j = 1; j < 14; j++)
			{
				if (kaidantable[i][j] >= 5)
					kaidantable[i][j] = 4;
			}
		}

		for (i = 0; i < 4; i++)
		{ //test
			for (j = 1; j < 14; j++)
			{
				if (pairtable[i][j] == 4)
					pairtable[i][j] = 3;
			}
		}

		if (qtyOfCards3(kaidantable) > 0)
		{

			for (kaidanlen = 11; kaidanlen > 2; kaidanlen--)
			{
				if (maisuu > 2)
					break;
				for (i = 0; i < 4; i++)
				{
					if (maisuu > 2)
						break;
					for (j = 1; j < 14; j++)
					{
						if (maisuu > 2)
							break;
						if (kaidantable[i][j] == kaidanlen)
						{
							for (k = 0; k < kaidanlen; k++)
							{

								if (count == 0)
								{
									state.kumi0[i][j + k] = copytable[i][j + k];
								}
								else if (count == 1)
								{
									state.kumi1[i][j + k] = copytable[i][j + k];
								}
								else if (count == 2)
								{
									state.kumi2[i][j + k] = copytable[i][j + k];
								}
								else if (count == 3)
								{
									state.kumi3[i][j + k] = copytable[i][j + k];
								}
								else if (count == 4)
								{
									state.kumi4[i][j + k] = copytable[i][j + k];
								}
								else if (count == 5)
								{
									state.kumi5[i][j + k] = copytable[i][j + k];
								}
								else if (count == 6)
								{
									state.kumi6[i][j + k] = copytable[i][j + k];
								}
								else if (count == 7)
								{
									state.kumi7[i][j + k] = copytable[i][j + k];
								}
								else if (count == 8)
								{
									state.kumi8[i][j + k] = copytable[i][j + k];
								}
								else if (count == 9)
								{
									state.kumi9[i][j + k] = copytable[i][j + k];
								}
								else if (count == 10)
								{
									state.kumi10[i][j + k] = copytable[i][j + k];
								}
								else if (count == 11)
								{
									state.kumi11[i][j + k] = copytable[i][j + k];
								}
								else if (count == 12)
								{
									state.kumi12[i][j + k] = copytable[i][j + k];
								}
								copytable[i][j + k] = 0;
							}

							if (count == 0)
							{
								maisuu = qtyOfCards(state.kumi0);
							}
							else if (count == 1)
							{
								maisuu = qtyOfCards(state.kumi1);
							}
							else if (count == 2)
							{
								maisuu = qtyOfCards(state.kumi2);
							}
							else if (count == 3)
							{
								maisuu = qtyOfCards(state.kumi3);
							}
							else if (count == 4)
							{
								maisuu = qtyOfCards(state.kumi4);
							}
							else if (count == 5)
							{
								maisuu = qtyOfCards(state.kumi5);
							}
							else if (count == 6)
							{
								maisuu = qtyOfCards(state.kumi6);
							}
							else if (count == 7)
							{
								maisuu = qtyOfCards(state.kumi7);
							}
							else if (count == 8)
							{
								maisuu = qtyOfCards(state.kumi8);
							}
							else if (count == 9)
							{
								maisuu = qtyOfCards(state.kumi9);
							}
							else if (count == 10)
							{
								maisuu = qtyOfCards(state.kumi10);
							}
							else if (count == 11)
							{
								maisuu = qtyOfCards(state.kumi11);
							}
							else if (count == 12)
							{
								maisuu = qtyOfCards(state.kumi12);
							}
						}
					}
				}
			}
			if (count == 0)
			{
				point = setvalue_kaidan(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_kaidan(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_kaidan(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_kaidan(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_kaidan(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_kaidan(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_kaidan(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_kaidan(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_kaidan(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_kaidan(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_kaidan(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_kaidan(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_kaidan(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		else if (qtyOfCards2(pairtable) > 0)
		{

			//maisuu=0;

			for (pairmai = 5; pairmai > 1; pairmai--)
			{
				if (maisuu > 1)
					break;
				for (i = 0; i < 4; i++)
				{
					if (maisuu > 1)
						break;
					for (j = 1; j < 14; j++)
					{
						if (maisuu > 1)
							break;
						if (pairtable[i][j] == pairmai)
						{

							for (k = 0; k < 4; k++)
							{
								if (copytable[k][j] == 1 || copytable[k][j] == 2)
								{

									if (count == 0)
									{
										state.kumi0[k][j] = copytable[k][j];
									}
									else if (count == 1)
									{
										state.kumi1[k][j] = copytable[k][j];
									}
									else if (count == 2)
									{
										state.kumi2[k][j] = copytable[k][j];
									}
									else if (count == 3)
									{
										state.kumi3[k][j] = copytable[k][j];
									}
									else if (count == 4)
									{
										;
										state.kumi4[k][j] = copytable[k][j];
									}
									else if (count == 5)
									{
										state.kumi5[k][j] = copytable[k][j];
									}
									else if (count == 6)
									{
										state.kumi6[k][j] = copytable[k][j];
									}
									else if (count == 7)
									{
										state.kumi7[k][j] = copytable[k][j];
									}
									else if (count == 8)
									{
										state.kumi8[k][j] = copytable[k][j];
									}
									else if (count == 9)
									{
										state.kumi9[k][j] = copytable[k][j];
									}
									else if (count == 10)
									{
										state.kumi10[k][j] = copytable[k][j];
									}
									else if (count == 11)
									{
										state.kumi11[k][j] = copytable[k][j];
									}
									else if (count == 12)
									{
										state.kumi12[k][j] = copytable[k][j];
									}

									copytable[k][j] = 0;

									count2 = count2 + 1;

									if (count2 == pairmai)
									{
										count2 = 0;
										break;
									}
								}
							}

							if (count == 0)
							{
								maisuu = qtyOfCards(state.kumi0);
							}
							else if (count == 1)
							{
								maisuu = qtyOfCards(state.kumi1);
							}
							else if (count == 2)
							{
								maisuu = qtyOfCards(state.kumi2);
							}
							else if (count == 3)
							{
								maisuu = qtyOfCards(state.kumi3);
							}
							else if (count == 4)
							{
								maisuu = qtyOfCards(state.kumi4);
							}
							else if (count == 5)
							{
								maisuu = qtyOfCards(state.kumi5);
							}
							else if (count == 6)
							{
								maisuu = qtyOfCards(state.kumi6);
							}
							else if (count == 7)
							{
								maisuu = qtyOfCards(state.kumi7);
							}
							else if (count == 8)
							{
								maisuu = qtyOfCards(state.kumi8);
							}
							else if (count == 9)
							{
								maisuu = qtyOfCards(state.kumi9);
							}
							else if (count == 10)
							{
								maisuu = qtyOfCards(state.kumi10);
							}
							else if (count == 11)
							{
								maisuu = qtyOfCards(state.kumi11);
							}
							else if (count == 12)
							{
								maisuu = qtyOfCards(state.kumi12);
							}
						}
					}
				}
			}
			if (count == 0)
			{
				point = setvalue_pair(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_pair(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_pair(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_pair(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_pair(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_pair(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_pair(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_pair(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_pair(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_pair(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_pair(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_pair(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_pair(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		else if (qtyOfCards(copytable) > 0)
		{

			for (i = 0; i < 4; i++)
			{
				if (maisuu == 1)
					break;
				for (j = 1; j < 14; j++)
				{
					if (maisuu == 1)
						break;
					if (copytable[i][j] == 1 || copytable[i][j] == 2)
					{

						if (count == 0)
						{
							state.kumi0[i][j] = copytable[i][j];
						}
						else if (count == 1)
						{
							state.kumi1[i][j] = copytable[i][j];
						}
						else if (count == 2)
						{
							state.kumi2[i][j] = copytable[i][j];
						}
						else if (count == 3)
						{
							state.kumi3[i][j] = copytable[i][j];
						}
						else if (count == 4)
						{
							state.kumi4[i][j] = copytable[i][j];
						}
						else if (count == 5)
						{
							state.kumi5[i][j] = copytable[i][j];
						}
						else if (count == 6)
						{
							state.kumi6[i][j] = copytable[i][j];
						}
						else if (count == 7)
						{
							state.kumi7[i][j] = copytable[i][j];
						}
						else if (count == 8)
						{
							state.kumi8[i][j] = copytable[i][j];
						}
						else if (count == 9)
						{
							state.kumi9[i][j] = copytable[i][j];
						}
						else if (count == 10)
						{
							state.kumi10[i][j] = copytable[i][j];
						}
						else if (count == 11)
						{
							state.kumi11[i][j] = copytable[i][j];
						}
						else if (count == 12)
						{
							state.kumi12[i][j] = copytable[i][j];
						}

						copytable[i][j] = 0;

						if (count == 0)
						{
							maisuu = qtyOfCards(state.kumi0);
						}
						else if (count == 1)
						{
							maisuu = qtyOfCards(state.kumi1);
						}
						else if (count == 2)
						{
							maisuu = qtyOfCards(state.kumi2);
						}
						else if (count == 3)
						{
							maisuu = qtyOfCards(state.kumi3);
						}
						else if (count == 4)
						{
							maisuu = qtyOfCards(state.kumi4);
						}
						else if (count == 5)
						{
							maisuu = qtyOfCards(state.kumi5);
						}
						else if (count == 6)
						{
							maisuu = qtyOfCards(state.kumi6);
						}
						else if (count == 7)
						{
							maisuu = qtyOfCards(state.kumi7);
						}
						else if (count == 8)
						{
							maisuu = qtyOfCards(state.kumi8);
						}
						else if (count == 9)
						{
							maisuu = qtyOfCards(state.kumi9);
						}
						else if (count == 10)
						{
							maisuu = qtyOfCards(state.kumi10);
						}
						else if (count == 11)
						{
							maisuu = qtyOfCards(state.kumi11);
						}
						else if (count == 12)
						{
							maisuu = qtyOfCards(state.kumi12);
						}
					}
				}
			}

			if (maisuu == 0 && joker_check(copytable) == 2)
			{
				if (count == 0)
				{
					state.kumi0[4][1] = 2;
				}
				else if (count == 1)
				{
					state.kumi1[4][1] = 2;
				}
				else if (count == 2)
				{
					state.kumi2[4][1] = 2;
				}
				else if (count == 3)
				{
					state.kumi3[4][1] = 2;
				}
				else if (count == 4)
				{
					state.kumi4[4][1] = 2;
				}
				else if (count == 5)
				{
					state.kumi5[4][1] = 2;
				}
				else if (count == 6)
				{
					state.kumi6[4][1] = 2;
				}
				else if (count == 7)
				{
					state.kumi7[4][1] = 2;
				}
				else if (count == 8)
				{
					state.kumi8[4][1] = 2;
				}
				else if (count == 9)
				{
					state.kumi9[4][1] = 2;
				}
				else if (count == 10)
				{
					state.kumi10[4][1] = 2;
				}
				else if (count == 11)
				{
					state.kumi11[4][1] = 2;
				}
				else if (count == 12)
				{
					state.kumi12[4][1] = 2;
				}
				copytable[4][1] = 0;
			}

			if (count == 0)
			{
				point = setvalue_single(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_single(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_single(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_single(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_single(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_single(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_single(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_single(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_single(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_single(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_single(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_single(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_single(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		/*printf("kumi:%d   point:%d\n",count,point);
		
						for(i=0;i<5;i++){
    	 					for(j=0;j<15;j++){
      						
    	 						if(count==0){
    	 							printf("%i ",state.kumi0[i][j]);
								}else if(count==1){
									printf("%i ",state.kumi1[i][j]);
								}else if(count==2){
									printf("%i ",state.kumi2[i][j]);
								}else if(count==3){
									printf("%i ",state.kumi3[i][j]);
								}else if(count==4){
									printf("%i ",state.kumi4[i][j]);
								}else if(count==5){
									printf("%i ",state.kumi5[i][j]);
								}else if(count==6){
									printf("%i ",state.kumi6[i][j]);
								}else if(count==7){
									printf("%i ",state.kumi7[i][j]);
								}else if(count==8){
									printf("%i ",state.kumi8[i][j]);
								}else if(count==9){
									printf("%i ",state.kumi9[i][j]);
								}else if(count==10){
									printf("%i ",state.kumi10[i][j]);
								}
    						}
    						printf("\n");

						}

						printf("\n");*/

		count++;
	}
	setvalue_hands();
}

int bunkatsu_noprint2_a_rev(int table[8][15])
{
	int i, j, k, count = 0, point = 0, totalpoint = 0;
	int kaidantable[8][15] = {{0}};
	int pairtable[8][15] = {{0}};
	int copytable[8][15] = {{0}};
	int kaidanlen = 0;
	int pairmai = 0;
	int maisuu = 0;
	int c = 0, kumisuu = 0;
	int count2 = 0;

	//ペア→階段→単体

	clearTable(state.kumi0); //追加.組を保存する配列を一試合ごとにリセット。
	clearTable(state.kumi1);
	clearTable(state.kumi2);
	clearTable(state.kumi3);
	clearTable(state.kumi4);
	clearTable(state.kumi5);
	clearTable(state.kumi6);
	clearTable(state.kumi7);
	clearTable(state.kumi8);
	clearTable(state.kumi9);
	clearTable(state.kumi10);
	clearTable(state.kumi11);
	clearTable(state.kumi12);
	/*
						for(i=0;i<14;i++){
							clearkumi_info(i);
						}
						*/
	clearkumi_info2();
	for (i = 0; i < 5; i++)
	{
		for (j = 0; j < 15; j++)
		{
			copytable[i][j] = table[i][j]; //copytableの作成
		}
	}

	while (qtyOfCards(copytable) > 0)
	{

		maisuu = 0; //リセット

		for (i = 0; i < 4; i++)
		{ //階段tableの作成
			for (j = 13; j > 0 + 1; j--)
			{
				if (copytable[i][j] == 1 || copytable[i][j] == 2)
				{
					kaidantable[i][j] = kaidantable[i][j + 1] + 1;
				}
				else
				{
					kaidantable[i][j] = 0;
				}
			}
		}

		for (i = 0; i < 4; i++)
		{ //pairtableの作成
			for (j = 1 + 1; j < 14; j++)
			{
				if (copytable[i][j] == 1 || copytable[i][j] == 2)
				{
					pairtable[i][j] = copytable[0][j] + copytable[1][j] + copytable[2][j] + copytable[3][j];
					if (copytable[0][j] == 2 || copytable[1][j] == 2 || copytable[2][j] == 2 || copytable[3][j] == 2)
					{
						pairtable[i][j] = pairtable[i][j] - 1;
					}
				}
				else
				{
					pairtable[i][j] = 0;
				}
			}
		}

		for (i = 0; i < 4; i++)
		{ //test
			for (j = 1; j < 14; j++)
			{
				if (kaidantable[i][j] >= 5)
					kaidantable[i][j] = 4;
			}
		}

		for (i = 0; i < 4; i++)
		{ //test
			for (j = 1; j < 14; j++)
			{
				if (pairtable[i][j] == 4)
					pairtable[i][j] = 3;
			}
		}

		if (qtyOfCards2(pairtable) > 0)
		{

			//maisuu=0;

			for (pairmai = 5; pairmai > 1; pairmai--)
			{
				if (maisuu > 1)
					break;
				for (i = 0; i < 4; i++)
				{
					if (maisuu > 1)
						break;
					for (j = 1; j < 14; j++)
					{
						if (maisuu > 1)
							break;
						if (pairtable[i][j] == pairmai)
						{

							for (k = 0; k < 4; k++)
							{
								if (copytable[k][j] == 1 || copytable[k][j] == 2)
								{

									if (count == 0)
									{
										state.kumi0[k][j] = copytable[k][j];
									}
									else if (count == 1)
									{
										state.kumi1[k][j] = copytable[k][j];
									}
									else if (count == 2)
									{
										state.kumi2[k][j] = copytable[k][j];
									}
									else if (count == 3)
									{
										state.kumi3[k][j] = copytable[k][j];
									}
									else if (count == 4)
									{
										;
										state.kumi4[k][j] = copytable[k][j];
									}
									else if (count == 5)
									{
										state.kumi5[k][j] = copytable[k][j];
									}
									else if (count == 6)
									{
										state.kumi6[k][j] = copytable[k][j];
									}
									else if (count == 7)
									{
										state.kumi7[k][j] = copytable[k][j];
									}
									else if (count == 8)
									{
										state.kumi8[k][j] = copytable[k][j];
									}
									else if (count == 9)
									{
										state.kumi9[k][j] = copytable[k][j];
									}
									else if (count == 10)
									{
										state.kumi10[k][j] = copytable[k][j];
									}
									else if (count == 11)
									{
										state.kumi11[k][j] = copytable[k][j];
									}
									else if (count == 12)
									{
										state.kumi12[k][j] = copytable[k][j];
									}

									copytable[k][j] = 0;

									count2 = count2 + 1;

									if (count2 == pairmai)
									{
										count2 = 0;
										break;
									}
								}
							}

							if (count == 0)
							{
								maisuu = qtyOfCards(state.kumi0);
							}
							else if (count == 1)
							{
								maisuu = qtyOfCards(state.kumi1);
							}
							else if (count == 2)
							{
								maisuu = qtyOfCards(state.kumi2);
							}
							else if (count == 3)
							{
								maisuu = qtyOfCards(state.kumi3);
							}
							else if (count == 4)
							{
								maisuu = qtyOfCards(state.kumi4);
							}
							else if (count == 5)
							{
								maisuu = qtyOfCards(state.kumi5);
							}
							else if (count == 6)
							{
								maisuu = qtyOfCards(state.kumi6);
							}
							else if (count == 7)
							{
								maisuu = qtyOfCards(state.kumi7);
							}
							else if (count == 8)
							{
								maisuu = qtyOfCards(state.kumi8);
							}
							else if (count == 9)
							{
								maisuu = qtyOfCards(state.kumi9);
							}
							else if (count == 10)
							{
								maisuu = qtyOfCards(state.kumi10);
							}
							else if (count == 11)
							{
								maisuu = qtyOfCards(state.kumi11);
							}
							else if (count == 12)
							{
								maisuu = qtyOfCards(state.kumi12);
							}
						}
					}
				}
			}
			if (count == 0)
			{
				point = setvalue_pair(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_pair(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_pair(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_pair(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_pair(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_pair(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_pair(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_pair(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_pair(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_pair(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_pair(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_pair(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_pair(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		else if (qtyOfCards3(kaidantable) > 0)
		{

			for (kaidanlen = 11; kaidanlen > 2; kaidanlen--)
			{
				if (maisuu > 2)
					break;
				for (i = 0; i < 4; i++)
				{
					if (maisuu > 2)
						break;
					for (j = 1; j < 14; j++)
					{
						if (maisuu > 2)
							break;
						if (kaidantable[i][j] == kaidanlen)
						{
							for (k = 0; k < kaidanlen; k++)
							{

								if (count == 0)
								{
									state.kumi0[i][j + k] = copytable[i][j + k];
								}
								else if (count == 1)
								{
									state.kumi1[i][j + k] = copytable[i][j + k];
								}
								else if (count == 2)
								{
									state.kumi2[i][j + k] = copytable[i][j + k];
								}
								else if (count == 3)
								{
									state.kumi3[i][j + k] = copytable[i][j + k];
								}
								else if (count == 4)
								{
									state.kumi4[i][j + k] = copytable[i][j + k];
								}
								else if (count == 5)
								{
									state.kumi5[i][j + k] = copytable[i][j + k];
								}
								else if (count == 6)
								{
									state.kumi6[i][j + k] = copytable[i][j + k];
								}
								else if (count == 7)
								{
									state.kumi7[i][j + k] = copytable[i][j + k];
								}
								else if (count == 8)
								{
									state.kumi8[i][j + k] = copytable[i][j + k];
								}
								else if (count == 9)
								{
									state.kumi9[i][j + k] = copytable[i][j + k];
								}
								else if (count == 10)
								{
									state.kumi10[i][j + k] = copytable[i][j + k];
								}
								else if (count == 11)
								{
									state.kumi11[i][j + k] = copytable[i][j + k];
								}
								else if (count == 12)
								{
									state.kumi12[i][j + k] = copytable[i][j + k];
								}
								copytable[i][j + k] = 0;
							}

							if (count == 0)
							{
								maisuu = qtyOfCards(state.kumi0);
							}
							else if (count == 1)
							{
								maisuu = qtyOfCards(state.kumi1);
							}
							else if (count == 2)
							{
								maisuu = qtyOfCards(state.kumi2);
							}
							else if (count == 3)
							{
								maisuu = qtyOfCards(state.kumi3);
							}
							else if (count == 4)
							{
								maisuu = qtyOfCards(state.kumi4);
							}
							else if (count == 5)
							{
								maisuu = qtyOfCards(state.kumi5);
							}
							else if (count == 6)
							{
								maisuu = qtyOfCards(state.kumi6);
							}
							else if (count == 7)
							{
								maisuu = qtyOfCards(state.kumi7);
							}
							else if (count == 8)
							{
								maisuu = qtyOfCards(state.kumi8);
							}
							else if (count == 9)
							{
								maisuu = qtyOfCards(state.kumi9);
							}
							else if (count == 10)
							{
								maisuu = qtyOfCards(state.kumi10);
							}
							else if (count == 11)
							{
								maisuu = qtyOfCards(state.kumi11);
							}
							else if (count == 12)
							{
								maisuu = qtyOfCards(state.kumi12);
							}
						}
					}
				}
			}
			if (count == 0)
			{
				point = setvalue_kaidan(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_kaidan(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_kaidan(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_kaidan(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_kaidan(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_kaidan(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_kaidan(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_kaidan(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_kaidan(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_kaidan(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_kaidan(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_kaidan(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_kaidan(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		else if (qtyOfCards(copytable) > 0)
		{

			for (i = 0; i < 4; i++)
			{
				if (maisuu == 1)
					break;
				for (j = 1; j < 14; j++)
				{
					if (maisuu == 1)
						break;
					if (copytable[i][j] == 1 || copytable[i][j] == 2)
					{

						if (count == 0)
						{
							state.kumi0[i][j] = copytable[i][j];
						}
						else if (count == 1)
						{
							state.kumi1[i][j] = copytable[i][j];
						}
						else if (count == 2)
						{
							state.kumi2[i][j] = copytable[i][j];
						}
						else if (count == 3)
						{
							state.kumi3[i][j] = copytable[i][j];
						}
						else if (count == 4)
						{
							state.kumi4[i][j] = copytable[i][j];
						}
						else if (count == 5)
						{
							state.kumi5[i][j] = copytable[i][j];
						}
						else if (count == 6)
						{
							state.kumi6[i][j] = copytable[i][j];
						}
						else if (count == 7)
						{
							state.kumi7[i][j] = copytable[i][j];
						}
						else if (count == 8)
						{
							state.kumi8[i][j] = copytable[i][j];
						}
						else if (count == 9)
						{
							state.kumi9[i][j] = copytable[i][j];
						}
						else if (count == 10)
						{
							state.kumi10[i][j] = copytable[i][j];
						}
						else if (count == 11)
						{
							state.kumi11[i][j] = copytable[i][j];
						}
						else if (count == 12)
						{
							state.kumi12[i][j] = copytable[i][j];
						}

						copytable[i][j] = 0;

						if (count == 0)
						{
							maisuu = qtyOfCards(state.kumi0);
						}
						else if (count == 1)
						{
							maisuu = qtyOfCards(state.kumi1);
						}
						else if (count == 2)
						{
							maisuu = qtyOfCards(state.kumi2);
						}
						else if (count == 3)
						{
							maisuu = qtyOfCards(state.kumi3);
						}
						else if (count == 4)
						{
							maisuu = qtyOfCards(state.kumi4);
						}
						else if (count == 5)
						{
							maisuu = qtyOfCards(state.kumi5);
						}
						else if (count == 6)
						{
							maisuu = qtyOfCards(state.kumi6);
						}
						else if (count == 7)
						{
							maisuu = qtyOfCards(state.kumi7);
						}
						else if (count == 8)
						{
							maisuu = qtyOfCards(state.kumi8);
						}
						else if (count == 9)
						{
							maisuu = qtyOfCards(state.kumi9);
						}
						else if (count == 10)
						{
							maisuu = qtyOfCards(state.kumi10);
						}
						else if (count == 11)
						{
							maisuu = qtyOfCards(state.kumi11);
						}
						else if (count == 12)
						{
							maisuu = qtyOfCards(state.kumi12);
						}
					}
				}
			}

			if (maisuu == 0 && joker_check(copytable) == 2)
			{
				if (count == 0)
				{
					state.kumi0[4][1] = 2;
				}
				else if (count == 1)
				{
					state.kumi1[4][1] = 2;
				}
				else if (count == 2)
				{
					state.kumi2[4][1] = 2;
				}
				else if (count == 3)
				{
					state.kumi3[4][1] = 2;
				}
				else if (count == 4)
				{
					state.kumi4[4][1] = 2;
				}
				else if (count == 5)
				{
					state.kumi5[4][1] = 2;
				}
				else if (count == 6)
				{
					state.kumi6[4][1] = 2;
				}
				else if (count == 7)
				{
					state.kumi7[4][1] = 2;
				}
				else if (count == 8)
				{
					state.kumi8[4][1] = 2;
				}
				else if (count == 9)
				{
					state.kumi9[4][1] = 2;
				}
				else if (count == 10)
				{
					state.kumi10[4][1] = 2;
				}
				else if (count == 11)
				{
					state.kumi11[4][1] = 2;
				}
				else if (count == 12)
				{
					state.kumi12[4][1] = 2;
				}
				copytable[4][1] = 0;
			}

			if (count == 0)
			{
				point = setvalue_single(state.kumi0, count);
			}
			else if (count == 1)
			{
				point = setvalue_single(state.kumi1, count);
			}
			else if (count == 2)
			{
				point = setvalue_single(state.kumi2, count);
			}
			else if (count == 3)
			{
				point = setvalue_single(state.kumi3, count);
			}
			else if (count == 4)
			{
				point = setvalue_single(state.kumi4, count);
			}
			else if (count == 5)
			{
				point = setvalue_single(state.kumi5, count);
			}
			else if (count == 6)
			{
				point = setvalue_single(state.kumi6, count);
			}
			else if (count == 7)
			{
				point = setvalue_single(state.kumi7, count);
			}
			else if (count == 8)
			{
				point = setvalue_single(state.kumi8, count);
			}
			else if (count == 9)
			{
				point = setvalue_single(state.kumi9, count);
			}
			else if (count == 10)
			{
				point = setvalue_single(state.kumi10, count);
			}
			else if (count == 11)
			{
				point = setvalue_single(state.kumi11, count);
			}
			else if (count == 12)
			{
				point = setvalue_single(state.kumi12, count);
			}
			totalpoint = totalpoint + point;
		}
		/*printf("kumi:%d   point:%d\n",count,point);
		
						for(i=0;i<5;i++){
    	 					for(j=0;j<15;j++){
      						
    	 						if(count==0){
    	 							printf("%i ",state.kumi0[i][j]);
								}else if(count==1){
									printf("%i ",state.kumi1[i][j]);
								}else if(count==2){
									printf("%i ",state.kumi2[i][j]);
								}else if(count==3){
									printf("%i ",state.kumi3[i][j]);
								}else if(count==4){
									printf("%i ",state.kumi4[i][j]);
								}else if(count==5){
									printf("%i ",state.kumi5[i][j]);
								}else if(count==6){
									printf("%i ",state.kumi6[i][j]);
								}else if(count==7){
									printf("%i ",state.kumi7[i][j]);
								}else if(count==8){
									printf("%i ",state.kumi8[i][j]);
								}else if(count==9){
									printf("%i ",state.kumi9[i][j]);
								}else if(count==10){
									printf("%i ",state.kumi10[i][j]);
								}
    						}
    						printf("\n");

						}

						printf("\n");*/

		count++;
	}
	setvalue_hands();
}

int bunkatsu_pre2(int table[8][15])
{

	int i[13] = {0};
	int j[13] = {0};
	int a = 0, b = 0, c = 0, d = 0, count = 0, f = 20, g = 20;
	int copy_table[8][15] = {{0}};
	int max_value = 0, h = 0, hh = 0, k = 20, l = 20, flag = 0;
	int kumisuu = 20, kumisuu2 = 20, kumisuu3 = 20, max_value2 = 0, o = 20, p = 20, flag2 = 0;
	int rank = 0, flag3 = 0, q = 20, r = 20;
	int count2 = 0;
	int kimeta = 0;
	int start_value = 0, start_kumisuu3 = 0;
	//int max_value_count=0;
	int joker_table[8][15] = {{0}};

	int max_value3 = 0;
	int s = 0, t = 0, flag4 = 0;
	int rank2 = 0;

	int flag_kako = 0;
	int max_value4 = 0;
	/*
	int x=3;
	int y=3;
	*/

	if (state.rev == 0)
	{
		//h=4;
		//h=17;
		h = 19;
		hh = 6;
		rank2 = 0;
	}
	else
	{
		//h=5;
		//h=18;
		h = 20;
		hh = 7;
		rank2 = 14;
	}

	bunkatsu_all2(table); //JOKER単体用

	max_value = state.kumi_info[13][h];
	max_value2 = state.kumi_info[13][h];
	max_value3 = state.kumi_info[13][h];
	max_value4 = state.kumi_info[13][h];
	start_value = state.kumi_info[13][h];
	kumisuu = state.kumi_info[13][0];
	kumisuu2 = state.kumi_info[13][0];

	kumisuu3 = state.kumi_info[13][0] - state.kumi_info[13][hh];
	start_kumisuu3 = state.kumi_info[13][0] - state.kumi_info[13][hh];

	if (table[4][1] == 2)
	{
		/*
		if(state.game_count==4406){
			printf("bunkatsu4\n");
		}
		*/

		for (a = 0; a < 4; a++)
		{
			for (b = 0; b < 15; b++)
			{
				copy_table[a][b] = table[a][b]; //copytableの作成（JOKERをコピーしない）
			}
		}

		for (b = 0; b < 15; b++)
		{
			for (a = 0; a < 4; a++)
			{ //各カードの場所を保存(JOKERを含まない）
				if (copy_table[a][b] == 1)
				{
					i[c] = a;
					j[c] = b;
					c = c + 1;
				}
			}
		}
		d = c; //cが9ならば[0]から[8]の9回繰り返されたということ

		//outputTable(table);
		//outputTable(copy_table);

		if (flag_kako == 0)
		{ //flag_kakoは結局、用いなかった

			if (state.finish == 0)
			{
				if (qtyOfCards5(copy_table, 13) >= 1 && state.rev == 0)
				{
					joker_table[0][13] = 2;
					joker_table[1][13] = 2;
					joker_table[2][13] = 2;
					joker_table[3][13] = 2;
				}

				if (qtyOfCards5(copy_table, 1) >= 1 && state.rev == 1)
				{
					joker_table[0][1] = 2;
					joker_table[1][1] = 2;
					joker_table[2][1] = 2;
					joker_table[3][1] = 2;
				}
			}

			for (c = 0; c < d; c++)
			{
				//printf("test4\n");
				//マークに対応
				if (copy_table[0][j[c]] == 0 && joker_table[0][j[c]] == 0)
				{
					copy_table[0][j[c]] = 2;
					joker_table[0][j[c]] = 2;

					bunkatsu_all2(copy_table);
					count = count + 1;

					if (state.kumi_info[13][h] >= max_value)
					{
						max_value = state.kumi_info[13][h];
						///max_value_count=count;
						f = 0;
						g = j[c];
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] <= 1)
					{
						k = 0;
						l = j[c];

						flag = 1;
					}

					if (state.kumi_info[13][0] <= kumisuu2)
					{
						kumisuu2 = state.kumi_info[13][0];
						max_value2 = state.kumi_info[13][h];

						o = 0;
						p = j[c];

						flag2 = 1;
					}

					if (rank <= j[c])
					{
						rank = j[c];
						max_value3 = state.kumi_info[13][h];

						q = 0;
						r = j[c];

						flag3 = 1;
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] < kumisuu3)
					{
						kumisuu3 = state.kumi_info[13][0];
						max_value4 = state.kumi_info[13][h];
						rank2 = j[c];

						s = 0;
						t = j[c];

						flag4 = 1;
					}
					/*
					if(state.rev==0){
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2<j[c]){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=0;
							t=j[c];
							
							flag4=1;
						}
					}else{
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2>j[c]){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=0;
							t=j[c];
							
							flag4=1;
						}
					}
					*/

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] == kumisuu3 && state.kumi_info[13][h] > max_value4)
					{
						kumisuu3 = state.kumi_info[13][0];
						max_value4 = state.kumi_info[13][h];
						//rank2=j[c];

						s = 0;
						t = j[c];

						flag4 = 1;
					}

					copy_table[0][j[c]] = 0;
				}

				if (copy_table[1][j[c]] == 0 && joker_table[1][j[c]] == 0)
				{
					copy_table[1][j[c]] = 2;
					joker_table[1][j[c]] = 2;

					bunkatsu_all2(copy_table);
					count = count + 1;

					if (state.kumi_info[13][h] >= max_value)
					{
						max_value = state.kumi_info[13][h];
						//max_value_count=count;
						f = 1;
						g = j[c];
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] <= 1)
					{
						k = 1;
						l = j[c];

						flag = 1;
					}

					if (state.kumi_info[13][0] <= kumisuu2)
					{
						kumisuu2 = state.kumi_info[13][0];
						max_value2 = state.kumi_info[13][h];

						o = 1;
						p = j[c];

						flag2 = 1;
					}

					if (rank <= j[c])
					{
						rank = j[c];
						max_value3 = state.kumi_info[13][h];

						q = 1;
						r = j[c];

						flag3 = 1;
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] < kumisuu3)
					{
						kumisuu3 = state.kumi_info[13][0];
						rank2 = j[c];

						s = 1;
						t = j[c];

						flag4 = 1;
					}

					/*
					if(state.rev==0){
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2<j[c]){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=1;
							t=j[c];
							
							flag4=1;
						}
					}else{
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2>j[c]){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=1;
							t=j[c];
							
							flag4=1;
						}
					}
					*/
					if (state.kumi_info[13][0] - state.kumi_info[13][hh] == kumisuu3 && state.kumi_info[13][h] > max_value4)
					{
						kumisuu3 = state.kumi_info[13][0];
						max_value4 = state.kumi_info[13][h];
						//rank2=j[c];

						s = 1;
						t = j[c];

						flag4 = 1;
					}

					copy_table[1][j[c]] = 0;
				}

				if (copy_table[2][j[c]] == 0 && joker_table[2][j[c]] == 0)
				{
					copy_table[2][j[c]] = 2;
					joker_table[2][j[c]] = 2;

					bunkatsu_all2(copy_table);
					count = count + 1;

					if (state.kumi_info[13][h] >= max_value)
					{
						max_value = state.kumi_info[13][h];
						//max_value_count=count;
						f = 2;
						g = j[c];
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] <= 1)
					{
						k = 2;
						l = j[c];

						flag = 1;
					}

					if (state.kumi_info[13][0] <= kumisuu2)
					{
						kumisuu2 = state.kumi_info[13][0];
						max_value2 = state.kumi_info[13][h];

						o = 2;
						p = j[c];

						flag2 = 1;
					}

					if (rank <= j[c])
					{
						rank = j[c];
						max_value3 = state.kumi_info[13][h];

						q = 2;
						r = j[c];

						flag3 = 1;
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] < kumisuu3)
					{
						kumisuu3 = state.kumi_info[13][0];
						rank2 = j[c];

						s = 2;
						t = j[c];

						flag4 = 1;
					}
					/*
					if(state.rev==0){
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2<j[c]){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=2;
							t=j[c];
							
							flag4=1;
						}
					}else{
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2>j[c]){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=2;
							t=j[c];
							
							flag4=1;
						}
					}
					*/

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] == kumisuu3 && state.kumi_info[13][h] > max_value4)
					{
						kumisuu3 = state.kumi_info[13][0];
						max_value4 = state.kumi_info[13][h];
						//rank2=j[c];

						s = 2;
						t = j[c];

						flag4 = 1;
					}

					copy_table[2][j[c]] = 0;
				}

				if (copy_table[3][j[c]] == 0 && joker_table[3][j[c]] == 0)
				{
					copy_table[3][j[c]] = 2;
					joker_table[3][j[c]] = 2;

					bunkatsu_all2(copy_table);
					count = count + 1;

					if (state.kumi_info[13][h] >= max_value)
					{
						max_value = state.kumi_info[13][h];
						//max_value_count=count;
						f = 3;
						g = j[c];
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] <= 1)
					{
						k = 3;
						l = j[c];

						flag = 1;
					}

					if (state.kumi_info[13][0] <= kumisuu2)
					{
						kumisuu2 = state.kumi_info[13][0];
						max_value2 = state.kumi_info[13][h];

						o = 3;
						p = j[c];

						flag2 = 1;
					}

					if (rank <= j[c])
					{
						rank = j[c];
						max_value3 = state.kumi_info[13][h];

						q = 3;
						r = j[c];

						flag3 = 1;
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] < kumisuu3)
					{
						kumisuu3 = state.kumi_info[13][0];
						rank2 = j[c];

						s = 3;
						t = j[c];

						flag4 = 1;
					}
					/*
					if(state.rev==0){
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2<j[c]){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=3;
							t=j[c];
							
							flag4=1;
						}
					}else{
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2>j[c]){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=3;
							t=j[c];
							
							flag4=1;
						}
					}
					*/
					if (state.kumi_info[13][0] - state.kumi_info[13][hh] == kumisuu3 && state.kumi_info[13][h] > max_value4)
					{
						kumisuu3 = state.kumi_info[13][0];
						max_value4 = state.kumi_info[13][h];
						//rank2=j[c];

						s = 3;
						t = j[c];

						flag4 = 1;
					}
					copy_table[3][j[c]] = 0;
				}
				//ここまで

				if (state.finish == 0)
				{
					if (qtyOfCards5(copy_table, 13) >= 1 && state.rev == 0)
					{
						joker_table[0][13] = 0;
						joker_table[1][13] = 0;
						joker_table[2][13] = 0;
						joker_table[3][13] = 0;
					}

					if (qtyOfCards5(copy_table, 1) >= 1 && state.rev == 1)
					{
						joker_table[0][1] = 0;
						joker_table[1][1] = 0;
						joker_table[2][1] = 0;
						joker_table[3][1] = 0;
					}
				}

				//前後の数字に対応
				if (j[c] >= 2 && j[c] <= 12 && copy_table[i[c]][j[c] + 1] == 1 && copy_table[i[c]][j[c] - 1] == 0 && joker_table[i[c]][j[c] - 1] == 0)
				{
					copy_table[i[c]][j[c] - 1] = 2;
					joker_table[i[c]][j[c] - 1] = 2;

					bunkatsu_all2(copy_table);
					count = count + 1;

					if (state.kumi_info[13][h] >= max_value)
					{
						max_value = state.kumi_info[13][h];
						//max_value_count=count;
						f = i[c];
						g = j[c] - 1;
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] <= 1)
					{
						k = i[c];
						l = j[c] - 1;

						flag = 1;
					}

					if (state.kumi_info[13][0] <= kumisuu2)
					{
						kumisuu2 = state.kumi_info[13][0];
						max_value2 = state.kumi_info[13][h];

						o = i[c];
						p = j[c] - 1;

						flag2 = 1;
					}

					if (rank <= j[c] - 1)
					{
						rank = j[c] - 1;
						max_value3 = state.kumi_info[13][h];

						q = i[c];
						r = j[c] - 1;

						flag3 = 1;
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] < kumisuu3)
					{
						kumisuu3 = state.kumi_info[13][0];
						rank2 = j[c];

						s = i[c];
						t = j[c] - 1;

						flag4 = 1;
					}

					/*
					if(state.rev==0){
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2<j[c]-1){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=i[c];
							t=j[c]-1;
							
							flag4=1;
						}
					}else{
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2>j[c]-1){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=i[c];
							t=j[c]-1;
							
							flag4=1;
						}
					}
					*/

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] == kumisuu3 && state.kumi_info[13][h] > max_value4)
					{
						kumisuu3 = state.kumi_info[13][0];
						max_value4 = state.kumi_info[13][h];
						//rank2=j[c];

						s = i[c];
						t = j[c] - 1;

						flag4 = 1;
					}

					copy_table[i[c]][j[c] - 1] = 0;
				}

				if (j[c] >= 3 && j[c] <= 13 && copy_table[i[c]][j[c] - 2] == 1 && copy_table[i[c]][j[c] - 1] == 0 && joker_table[i[c]][j[c] - 1] == 0)
				{
					copy_table[i[c]][j[c] - 1] = 2;
					joker_table[i[c]][j[c] - 1] = 2;

					bunkatsu_all2(copy_table);
					count = count + 1;

					if (state.kumi_info[13][h] >= max_value)
					{
						max_value = state.kumi_info[13][h];
						//max_value_count=count;
						f = i[c];
						g = j[c] - 1;
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] <= 1)
					{
						k = i[c];
						l = j[c] - 1;

						flag = 1;
					}

					if (state.kumi_info[13][0] <= kumisuu2)
					{
						kumisuu2 = state.kumi_info[13][0];
						max_value2 = state.kumi_info[13][h];

						o = i[c];
						p = j[c] - 1;

						flag2 = 1;
					}

					if (rank <= j[c] - 1)
					{
						rank = j[c] - 1;
						max_value3 = state.kumi_info[13][h];

						q = i[c];
						r = j[c] - 1;

						flag3 = 1;
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] < kumisuu3)
					{
						kumisuu3 = state.kumi_info[13][0];
						rank2 = j[c];

						s = i[c];
						t = j[c] - 1;

						flag4 = 1;
					}
					/*
					if(state.rev==0){
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2<j[c]-1){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=i[c];
							t=j[c]-1;
							
							flag4=1;
						}
					}else{
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2>j[c]-1){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=i[c];
							t=j[c]-1;
							
							flag4=1;
						}
					}
					*/

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] == kumisuu3 && state.kumi_info[13][h] > max_value4)
					{
						kumisuu3 = state.kumi_info[13][0];
						max_value4 = state.kumi_info[13][h];
						//rank2=j[c];

						s = i[c];
						t = j[c] - 1;

						flag4 = 1;
					}

					copy_table[i[c]][j[c] - 1] = 0;
				}

				if (j[c] >= 2 && j[c] <= 12 && copy_table[i[c]][j[c] - 1] == 1 && copy_table[i[c]][j[c] + 1] == 0 && joker_table[i[c]][j[c] + 1] == 0)
				{
					copy_table[i[c]][j[c] + 1] = 2;
					joker_table[i[c]][j[c] + 1] = 2;

					bunkatsu_all2(copy_table);
					count = count + 1;

					if (state.kumi_info[13][h] >= max_value)
					{
						max_value = state.kumi_info[13][h];
						//max_value_count=count;
						f = i[c];
						g = j[c] + 1;
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] <= 1)
					{
						k = i[c];
						l = j[c] + 1;

						flag = 1;
					}

					if (state.kumi_info[13][0] <= kumisuu2)
					{
						kumisuu2 = state.kumi_info[13][0];
						max_value2 = state.kumi_info[13][h];

						o = i[c];
						p = j[c] + 1;

						flag2 = 1;
					}

					if (rank <= j[c] + 1)
					{
						rank = j[c] + 1;
						max_value3 = state.kumi_info[13][h];

						q = i[c];
						r = j[c] + 1;

						flag3 = 1;
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] < kumisuu3)
					{
						kumisuu3 = state.kumi_info[13][0];
						rank2 = j[c];

						s = i[c];
						t = j[c] + 1;

						flag4 = 1;
					}
					/*
					if(state.rev==0){
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2<j[c]+1){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=i[c];
							t=j[c]+1;
							
							flag4=1;
						}
					}else{
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2>j[c]+1){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=i[c];
							t=j[c]+1;
							
							flag4=1;
						}
					}
					*/

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] == kumisuu3 && state.kumi_info[13][h] > max_value4)
					{
						kumisuu3 = state.kumi_info[13][0];
						max_value4 = state.kumi_info[13][h];
						//rank2=j[c];

						s = i[c];
						t = j[c] + 1;

						flag4 = 1;
					}

					copy_table[i[c]][j[c] + 1] = 0;
				}

				if (j[c] >= 1 && j[c] <= 11 && copy_table[i[c]][j[c] + 2] == 1 && copy_table[i[c]][j[c] + 1] == 0 && joker_table[i[c]][j[c] + 1] == 0)
				{
					copy_table[i[c]][j[c] + 1] = 2;
					joker_table[i[c]][j[c] + 1] = 2;

					bunkatsu_all2(copy_table);
					count = count + 1;

					if (state.kumi_info[13][h] >= max_value)
					{
						max_value = state.kumi_info[13][h];
						//max_value_count=count;
						f = i[c];
						g = j[c] + 1;
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] <= 1)
					{
						k = i[c];
						l = j[c] + 1;

						flag = 1;
					}

					if (state.kumi_info[13][0] <= kumisuu2)
					{
						kumisuu2 = state.kumi_info[13][0];
						max_value2 = state.kumi_info[13][h];

						o = i[c];
						p = j[c] + 1;

						flag2 = 1;
					}

					if (rank <= j[c] + 1)
					{
						rank = j[c] + 1;
						max_value3 = state.kumi_info[13][h];

						q = i[c];
						r = j[c] + 1;

						flag3 = 1;
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] < kumisuu3)
					{
						kumisuu3 = state.kumi_info[13][0];
						rank2 = j[c];

						s = i[c];
						t = j[c] + 1;

						flag4 = 1;
					}
					/*
					if(state.rev==0){
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2<j[c]+1){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=i[c];
							t=j[c]+1;
							
							flag4=1;
						}
					}else{
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2>j[c]+1){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=i[c];
							t=j[c]+1;
							
							flag4=1;
						}
					}
					*/

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] == kumisuu3 && state.kumi_info[13][h] > max_value4)
					{
						kumisuu3 = state.kumi_info[13][0];
						max_value4 = state.kumi_info[13][h];
						//rank2=j[c];

						s = i[c];
						t = j[c] + 1;

						flag4 = 1;
					}

					copy_table[i[c]][j[c] + 1] = 0;
				}
			}

			//jokerの位置の決定、ここから
			/*
				if(state.game_count==4406){
					printf("bunkatsu5.5\n");
				}
			*/

			if (flag == 1 && k != 20 && l != 20)
			{ //<=1により、自分の番になれば勝てると判定できるとき

				copy_table[k][l] = 2;
				bunkatsu_all2(copy_table);
				//printf("k %d l %d\n",k,l);

				kimeta = 1;
				/*
					if(state.game_count==4406){
						printf("bunkatsu5.6\n");
						outputCards(copy_table);
					}
					*/
			}
			/*
				if(kimeta==0&&flag4==1){
					
					copy_table[s][t]=2;
					bunkatsu_all2(copy_table);
					
					kimeta=1;
				}
				*/
			/*
			if(kimeta==0){
				
				if(max_value2>=max_value3){
				
					if(flag2==1&&o!=20&&p!=20&&kumisuu2+1<kumisuu){

						copy_table[o][p]=2;
						bunkatsu_all2(copy_table);
						//printf("o %d p %d\n",o,p);
						
						kimeta=1;
							
					}
					
					if(flag3==1&&q!=20&&r!=20&&kimeta==0){
						
						copy_table[q][r]=2;
						bunkatsu_all2(copy_table);
						
						if(start_value>state.kumi_info[13][h]){
							copy_table[q][r]=0;
							copy_table[4][1]=2;
							bunkatsu_all2(copy_table);	
						}else{
							
						}
						
						kimeta=1;
						
					}
					
				}else{
					
					if(flag3==1&&q!=20&&r!=20){

						copy_table[q][r]=2;
						bunkatsu_all2(copy_table);
						
						if(start_value>state.kumi_info[13][h]){
							copy_table[q][r]=0;
							copy_table[4][1]=2;
							bunkatsu_all2(copy_table);	
						}else{
					
						}
						
						kimeta=1;
						
					}
					
				}
				
				
			}
			*/

			if (flag == 0 && f != 20 && g != 20 && kimeta == 0)
			{

				copy_table[f][g] = 2;
				bunkatsu_all2(copy_table);
				//outputCards(copy_table);
				//printf("f %d g %d\n",f,g);

				kimeta = 1;
			}

			if (kimeta == 0)
			{ //yobi
				copy_table[4][1] = 2;
				bunkatsu_all2(copy_table);
			}
			/*
				if(flag3==1&&q!=20&&r!=20&&kimeta==0){//予備
					
						printf("yobi\n");
					
						copy_table[q][r]=2;
						bunkatsu_all2(copy_table);
						
						if(start_value>state.kumi_info[13][h]){
							copy_table[q][r]=0;
							copy_table[4][1]=2;
							bunkatsu_all2(copy_table);	
							
						}else{
							
						}
						
						kimeta=1;
						
				}
				*/

			//joker_check2(copy_table);

			//printf("flag %d\n",flag);
			//printf("flag2 %d\n",flag2);
			//printf("flag3 %d\n",flag3);
			//printf("q %d r %d\n",q,r);

			//手札評価値を元にした分割は行わない事に。（仮）
			/*
				if(flag==0&&flag2==0&&f!=20&&g!=20&&kimeta==0){

					copy_table[f][g]=2;
					bunkatsu_all2(copy_table);
					//outputCards(copy_table);
					//printf("f %d g %d\n",f,g);
					
					kimeta=1;
					
				}
				*/
			//printf("f %d g %d\n",f,g);

			//

			/*printf("kumi   point\n");
				outputCards(copy_table);
				
				if(qtyOfCards(state.kumi0)>=1)outputCards(state.kumi0);
				if(qtyOfCards(state.kumi1)>=1)outputCards(state.kumi1);
				if(qtyOfCards(state.kumi2)>=1)outputCards(state.kumi2);
				if(qtyOfCards(state.kumi3)>=1)outputCards(state.kumi3);
				if(qtyOfCards(state.kumi4)>=1)outputCards(state.kumi4);
				if(qtyOfCards(state.kumi5)>=1)outputCards(state.kumi5);
				if(qtyOfCards(state.kumi6)>=1)outputCards(state.kumi6);
				if(qtyOfCards(state.kumi7)>=1)outputCards(state.kumi7);
				if(qtyOfCards(state.kumi8)>=1)outputCards(state.kumi8);
				if(qtyOfCards(state.kumi9)>=1)outputCards(state.kumi9);
				if(qtyOfCards(state.kumi10)>=1)outputCards(state.kumi10);
				*/

			//
			/*
				printf("kumisuu2   %d\n",state.kumi_info[13][0]);
				printf("joker[o][p]   joker[%d][%d]\n",o,p);
				printf("max_value2   %d\n",max_value2);
				printf("max_value3   %d\n",max_value3);
			
				joker_check3(copy_table);
				*/
			//outputTable(copy_table);
			//outputTable(joker_table);
		}
	}
	/*
	if(state.game_count==4406){
			printf("bunkatsu5\n");
			outputCards(copy_table);
		}
	*/

	return max_value;
}
int bunkatsu_pre3(int table[8][15])
{ //手札交換用//使っていない部分も多数

	int i[13] = {0};
	int j[13] = {0};
	int a = 0, b = 0, c = 0, d = 0, count = 0, f = 20, g = 20;
	int copy_table[8][15] = {{0}};
	int max_value = 0, h = 0, hh = 0, k = 20, l = 20, flag = 0;
	int kumisuu = 20, kumisuu2 = 20, kumisuu3 = 20, max_value2 = 0, o = 20, p = 20, flag2 = 0;
	int rank = 0, flag3 = 0, q = 20, r = 20;
	int count2 = 0;
	int kimeta = 0;
	int start_value = 0, start_kumisuu3 = 0;
	//int max_value_count=0;
	int joker_table[8][15] = {{0}};

	int max_value3 = 0;
	int s = 0, t = 0, flag4 = 0;
	int rank2 = 0;

	int flag_kako = 0;
	int max_value4 = 0;
	/*
	int x=3;
	int y=3;
	*/

	if (state.rev == 0)
	{
		h = 4;
		//h=17;
		//h=19;
		hh = 6;
		rank2 = 0;
	}
	else
	{
		h = 5;
		//h=18;
		//h=20;
		hh = 7;
		rank2 = 14;
	}

	bunkatsu_all2(table); //JOKER単体用

	max_value = state.kumi_info[13][h];
	max_value2 = state.kumi_info[13][h];
	max_value3 = state.kumi_info[13][h];
	max_value4 = state.kumi_info[13][h];
	start_value = state.kumi_info[13][h];
	kumisuu = state.kumi_info[13][0];
	kumisuu2 = state.kumi_info[13][0];

	kumisuu3 = state.kumi_info[13][0] - state.kumi_info[13][hh];
	start_kumisuu3 = state.kumi_info[13][0] - state.kumi_info[13][hh];

	if (table[4][1] == 2)
	{
		/*
		if(state.game_count==4406){
			printf("bunkatsu4\n");
		}
		*/

		for (a = 0; a < 4; a++)
		{
			for (b = 0; b < 15; b++)
			{
				copy_table[a][b] = table[a][b]; //copytableの作成（JOKERをコピーしない）
			}
		}

		for (b = 0; b < 15; b++)
		{
			for (a = 0; a < 4; a++)
			{ //各カードの場所を保存(JOKERを含まない）
				if (copy_table[a][b] == 1)
				{
					i[c] = a;
					j[c] = b;
					c = c + 1;
				}
			}
		}
		d = c; //cが9ならば[0]から[8]の9回繰り返されたということ

		//outputTable(table);
		//outputTable(copy_table);

		if (flag_kako == 0)
		{ //flag_kakoは結局、用いなかった

			if (state.finish == 0)
			{
				if (qtyOfCards5(copy_table, 13) >= 1 && state.rev == 0)
				{
					joker_table[0][13] = 2;
					joker_table[1][13] = 2;
					joker_table[2][13] = 2;
					joker_table[3][13] = 2;
				}

				if (qtyOfCards5(copy_table, 1) >= 1 && state.rev == 1)
				{
					joker_table[0][1] = 2;
					joker_table[1][1] = 2;
					joker_table[2][1] = 2;
					joker_table[3][1] = 2;
				}
			}

			for (c = 0; c < d; c++)
			{
				//printf("test4\n");
				//マークに対応
				if (copy_table[0][j[c]] == 0 && joker_table[0][j[c]] == 0)
				{
					copy_table[0][j[c]] = 2;
					joker_table[0][j[c]] = 2;

					bunkatsu_all2(copy_table);
					count = count + 1;

					if (state.kumi_info[13][h] >= max_value)
					{
						max_value = state.kumi_info[13][h];
						///max_value_count=count;
						f = 0;
						g = j[c];
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] <= 1)
					{
						k = 0;
						l = j[c];

						flag = 1;
					}
					if (state.kumi_info[13][0] == kumisuu2 && state.kumi_info[13][h] > max_value2)
					{
						kumisuu2 = state.kumi_info[13][0];
						max_value2 = state.kumi_info[13][h];

						o = 0;
						p = j[c];

						flag2 = 1;
					}
					if (state.kumi_info[13][0] < kumisuu2)
					{
						kumisuu2 = state.kumi_info[13][0];
						max_value2 = state.kumi_info[13][h];

						o = 0;
						p = j[c];

						flag2 = 1;
					}
					if (rank == j[c] && state.kumi_info[13][h] > max_value3)
					{
						rank = j[c];
						max_value3 = state.kumi_info[13][h];

						q = 0;
						r = j[c];

						flag3 = 1;
					}
					if (rank < j[c])
					{
						rank = j[c];
						max_value3 = state.kumi_info[13][h];

						q = 0;
						r = j[c];

						flag3 = 1;
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] < kumisuu3)
					{
						kumisuu3 = state.kumi_info[13][0];
						max_value4 = state.kumi_info[13][h];
						rank2 = j[c];

						s = 0;
						t = j[c];

						flag4 = 1;
					}
					/*
					if(state.rev==0){
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2<j[c]){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=0;
							t=j[c];
							
							flag4=1;
						}
					}else{
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2>j[c]){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=0;
							t=j[c];
							
							flag4=1;
						}
					}
					*/

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] == kumisuu3 && state.kumi_info[13][h] > max_value4)
					{
						kumisuu3 = state.kumi_info[13][0];
						max_value4 = state.kumi_info[13][h];
						//rank2=j[c];

						s = 0;
						t = j[c];

						flag4 = 1;
					}

					copy_table[0][j[c]] = 0;
				}

				if (copy_table[1][j[c]] == 0 && joker_table[1][j[c]] == 0)
				{
					copy_table[1][j[c]] = 2;
					joker_table[1][j[c]] = 2;

					bunkatsu_all2(copy_table);
					count = count + 1;

					if (state.kumi_info[13][h] >= max_value)
					{
						max_value = state.kumi_info[13][h];
						//max_value_count=count;
						f = 1;
						g = j[c];
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] <= 1)
					{
						k = 1;
						l = j[c];

						flag = 1;
					}
					if (state.kumi_info[13][0] == kumisuu2 && state.kumi_info[13][h] > max_value2)
					{
						kumisuu2 = state.kumi_info[13][0];
						max_value2 = state.kumi_info[13][h];

						o = 1;
						p = j[c];

						flag2 = 1;
					}
					if (state.kumi_info[13][0] < kumisuu2)
					{
						kumisuu2 = state.kumi_info[13][0];
						max_value2 = state.kumi_info[13][h];

						o = 1;
						p = j[c];

						flag2 = 1;
					}
					if (rank == j[c] && state.kumi_info[13][h] > max_value3)
					{
						rank = j[c];
						max_value3 = state.kumi_info[13][h];

						q = 1;
						r = j[c];

						flag3 = 1;
					}
					if (rank < j[c])
					{
						rank = j[c];
						max_value3 = state.kumi_info[13][h];

						q = 1;
						r = j[c];

						flag3 = 1;
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] < kumisuu3)
					{
						kumisuu3 = state.kumi_info[13][0];
						rank2 = j[c];

						s = 1;
						t = j[c];

						flag4 = 1;
					}

					/*
					if(state.rev==0){
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2<j[c]){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=1;
							t=j[c];
							
							flag4=1;
						}
					}else{
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2>j[c]){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=1;
							t=j[c];
							
							flag4=1;
						}
					}
					*/
					if (state.kumi_info[13][0] - state.kumi_info[13][hh] == kumisuu3 && state.kumi_info[13][h] > max_value4)
					{
						kumisuu3 = state.kumi_info[13][0];
						max_value4 = state.kumi_info[13][h];
						//rank2=j[c];

						s = 1;
						t = j[c];

						flag4 = 1;
					}

					copy_table[1][j[c]] = 0;
				}

				if (copy_table[2][j[c]] == 0 && joker_table[2][j[c]] == 0)
				{
					copy_table[2][j[c]] = 2;
					joker_table[2][j[c]] = 2;

					bunkatsu_all2(copy_table);
					count = count + 1;

					if (state.kumi_info[13][h] >= max_value)
					{
						max_value = state.kumi_info[13][h];
						//max_value_count=count;
						f = 2;
						g = j[c];
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] <= 1)
					{
						k = 2;
						l = j[c];

						flag = 1;
					}
					if (state.kumi_info[13][0] == kumisuu2 && state.kumi_info[13][h] > max_value2)
					{
						kumisuu2 = state.kumi_info[13][0];
						max_value2 = state.kumi_info[13][h];

						o = 2;
						p = j[c];

						flag2 = 1;
					}
					if (state.kumi_info[13][0] < kumisuu2)
					{
						kumisuu2 = state.kumi_info[13][0];
						max_value2 = state.kumi_info[13][h];

						o = 2;
						p = j[c];

						flag2 = 1;
					}
					if (rank == j[c] && state.kumi_info[13][h] > max_value3)
					{
						rank = j[c];
						max_value3 = state.kumi_info[13][h];

						q = 2;
						r = j[c];

						flag3 = 1;
					}
					if (rank < j[c])
					{
						rank = j[c];
						max_value3 = state.kumi_info[13][h];

						q = 2;
						r = j[c];

						flag3 = 1;
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] < kumisuu3)
					{
						kumisuu3 = state.kumi_info[13][0];
						rank2 = j[c];

						s = 2;
						t = j[c];

						flag4 = 1;
					}
					/*
					if(state.rev==0){
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2<j[c]){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=2;
							t=j[c];
							
							flag4=1;
						}
					}else{
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2>j[c]){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=2;
							t=j[c];
							
							flag4=1;
						}
					}
					*/

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] == kumisuu3 && state.kumi_info[13][h] > max_value4)
					{
						kumisuu3 = state.kumi_info[13][0];
						max_value4 = state.kumi_info[13][h];
						//rank2=j[c];

						s = 2;
						t = j[c];

						flag4 = 1;
					}

					copy_table[2][j[c]] = 0;
				}

				if (copy_table[3][j[c]] == 0 && joker_table[3][j[c]] == 0)
				{
					copy_table[3][j[c]] = 2;
					joker_table[3][j[c]] = 2;

					bunkatsu_all2(copy_table);
					count = count + 1;

					if (state.kumi_info[13][h] >= max_value)
					{
						max_value = state.kumi_info[13][h];
						//max_value_count=count;
						f = 3;
						g = j[c];
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] <= 1)
					{
						k = 3;
						l = j[c];

						flag = 1;
					}
					if (state.kumi_info[13][0] == kumisuu2 && state.kumi_info[13][h] > max_value2)
					{
						kumisuu2 = state.kumi_info[13][0];
						max_value2 = state.kumi_info[13][h];

						o = 3;
						p = j[c];

						flag2 = 1;
					}
					if (state.kumi_info[13][0] < kumisuu2)
					{
						kumisuu2 = state.kumi_info[13][0];
						max_value2 = state.kumi_info[13][h];

						o = 3;
						p = j[c];

						flag2 = 1;
					}
					if (rank == j[c] && state.kumi_info[13][h] > max_value3)
					{
						rank = j[c];
						max_value3 = state.kumi_info[13][h];

						q = 3;
						r = j[c];

						flag3 = 1;
					}
					if (rank < j[c])
					{
						rank = j[c];
						max_value3 = state.kumi_info[13][h];

						q = 3;
						r = j[c];

						flag3 = 1;
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] < kumisuu3)
					{
						kumisuu3 = state.kumi_info[13][0];
						rank2 = j[c];

						s = 3;
						t = j[c];

						flag4 = 1;
					}
					/*
					if(state.rev==0){
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2<j[c]){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=3;
							t=j[c];
							
							flag4=1;
						}
					}else{
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2>j[c]){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=3;
							t=j[c];
							
							flag4=1;
						}
					}
					*/
					if (state.kumi_info[13][0] - state.kumi_info[13][hh] == kumisuu3 && state.kumi_info[13][h] > max_value4)
					{
						kumisuu3 = state.kumi_info[13][0];
						max_value4 = state.kumi_info[13][h];
						//rank2=j[c];

						s = 3;
						t = j[c];

						flag4 = 1;
					}
					copy_table[3][j[c]] = 0;
				}
				//ここまで

				if (state.finish == 0)
				{
					if (qtyOfCards5(copy_table, 13) >= 1 && state.rev == 0)
					{
						joker_table[0][13] = 0;
						joker_table[1][13] = 0;
						joker_table[2][13] = 0;
						joker_table[3][13] = 0;
					}

					if (qtyOfCards5(copy_table, 1) >= 1 && state.rev == 1)
					{
						joker_table[0][1] = 0;
						joker_table[1][1] = 0;
						joker_table[2][1] = 0;
						joker_table[3][1] = 0;
					}
				}

				//前後の数字に対応
				if (j[c] >= 2 && j[c] <= 12 && copy_table[i[c]][j[c] + 1] == 1 && copy_table[i[c]][j[c] - 1] == 0 && joker_table[i[c]][j[c] - 1] == 0)
				{
					copy_table[i[c]][j[c] - 1] = 2;
					joker_table[i[c]][j[c] - 1] = 2;

					bunkatsu_all2(copy_table);
					count = count + 1;

					if (state.kumi_info[13][h] >= max_value)
					{
						max_value = state.kumi_info[13][h];
						//max_value_count=count;
						f = i[c];
						g = j[c] - 1;
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] <= 1)
					{
						k = i[c];
						l = j[c] - 1;

						flag = 1;
					}
					if (state.kumi_info[13][0] == kumisuu2 && state.kumi_info[13][h] > max_value2)
					{
						kumisuu2 = state.kumi_info[13][0];
						max_value2 = state.kumi_info[13][h];

						o = i[c];
						p = j[c] - 1;

						flag2 = 1;
					}
					if (state.kumi_info[13][0] < kumisuu2)
					{
						kumisuu2 = state.kumi_info[13][0];
						max_value2 = state.kumi_info[13][h];

						o = i[c];
						p = j[c] - 1;

						flag2 = 1;
					}
					if (rank == j[c] - 1 && state.kumi_info[13][h] > max_value3)
					{
						rank = j[c] - 1;
						max_value3 = state.kumi_info[13][h];

						q = i[c];
						r = j[c] - 1;

						flag3 = 1;
					}

					if (rank < j[c] - 1)
					{
						rank = j[c] - 1;
						max_value3 = state.kumi_info[13][h];

						q = i[c];
						r = j[c] - 1;

						flag3 = 1;
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] < kumisuu3)
					{
						kumisuu3 = state.kumi_info[13][0];
						rank2 = j[c];

						s = i[c];
						t = j[c] - 1;

						flag4 = 1;
					}

					/*
					if(state.rev==0){
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2<j[c]-1){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=i[c];
							t=j[c]-1;
							
							flag4=1;
						}
					}else{
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2>j[c]-1){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=i[c];
							t=j[c]-1;
							
							flag4=1;
						}
					}
					*/

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] == kumisuu3 && state.kumi_info[13][h] > max_value4)
					{
						kumisuu3 = state.kumi_info[13][0];
						max_value4 = state.kumi_info[13][h];
						//rank2=j[c];

						s = i[c];
						t = j[c] - 1;

						flag4 = 1;
					}

					copy_table[i[c]][j[c] - 1] = 0;
				}

				if (j[c] >= 3 && j[c] <= 13 && copy_table[i[c]][j[c] - 2] == 1 && copy_table[i[c]][j[c] - 1] == 0 && joker_table[i[c]][j[c] - 1] == 0)
				{
					copy_table[i[c]][j[c] - 1] = 2;
					joker_table[i[c]][j[c] - 1] = 2;

					bunkatsu_all2(copy_table);
					count = count + 1;

					if (state.kumi_info[13][h] >= max_value)
					{
						max_value = state.kumi_info[13][h];
						//max_value_count=count;
						f = i[c];
						g = j[c] - 1;
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] <= 1)
					{
						k = i[c];
						l = j[c] - 1;

						flag = 1;
					}
					if (state.kumi_info[13][0] == kumisuu2 && state.kumi_info[13][h] > max_value2)
					{
						kumisuu2 = state.kumi_info[13][0];
						max_value2 = state.kumi_info[13][h];

						o = i[c];
						p = j[c] - 1;

						flag2 = 1;
					}
					if (state.kumi_info[13][0] < kumisuu2)
					{
						kumisuu2 = state.kumi_info[13][0];
						max_value2 = state.kumi_info[13][h];

						o = i[c];
						p = j[c] - 1;

						flag2 = 1;
					}
					if (rank == j[c] - 1 && state.kumi_info[13][h] > max_value3)
					{
						rank = j[c] - 1;
						max_value3 = state.kumi_info[13][h];

						q = i[c];
						r = j[c] - 1;

						flag3 = 1;
					}

					if (rank < j[c] - 1)
					{
						rank = j[c] - 1;
						max_value3 = state.kumi_info[13][h];

						q = i[c];
						r = j[c] - 1;

						flag3 = 1;
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] < kumisuu3)
					{
						kumisuu3 = state.kumi_info[13][0];
						rank2 = j[c];

						s = i[c];
						t = j[c] - 1;

						flag4 = 1;
					}
					/*
					if(state.rev==0){
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2<j[c]-1){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=i[c];
							t=j[c]-1;
							
							flag4=1;
						}
					}else{
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2>j[c]-1){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=i[c];
							t=j[c]-1;
							
							flag4=1;
						}
					}
					*/

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] == kumisuu3 && state.kumi_info[13][h] > max_value4)
					{
						kumisuu3 = state.kumi_info[13][0];
						max_value4 = state.kumi_info[13][h];
						//rank2=j[c];

						s = i[c];
						t = j[c] - 1;

						flag4 = 1;
					}

					copy_table[i[c]][j[c] - 1] = 0;
				}

				if (j[c] >= 2 && j[c] <= 12 && copy_table[i[c]][j[c] - 1] == 1 && copy_table[i[c]][j[c] + 1] == 0 && joker_table[i[c]][j[c] + 1] == 0)
				{
					copy_table[i[c]][j[c] + 1] = 2;
					joker_table[i[c]][j[c] + 1] = 2;

					bunkatsu_all2(copy_table);
					count = count + 1;

					if (state.kumi_info[13][h] >= max_value)
					{
						max_value = state.kumi_info[13][h];
						//max_value_count=count;
						f = i[c];
						g = j[c] + 1;
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] <= 1)
					{
						k = i[c];
						l = j[c] + 1;

						flag = 1;
					}

					if (state.kumi_info[13][0] == kumisuu2 && state.kumi_info[13][h] > max_value2)
					{
						kumisuu2 = state.kumi_info[13][0];
						max_value2 = state.kumi_info[13][h];

						o = i[c];
						p = j[c] + 1;

						flag2 = 1;
					}

					if (state.kumi_info[13][0] < kumisuu2)
					{
						kumisuu2 = state.kumi_info[13][0];
						max_value2 = state.kumi_info[13][h];

						o = i[c];
						p = j[c] + 1;

						flag2 = 1;
					}

					if (rank == j[c] + 1 && state.kumi_info[13][h] > max_value3)
					{
						rank = j[c] + 1;
						max_value3 = state.kumi_info[13][h];

						q = i[c];
						r = j[c] + 1;

						flag3 = 1;
					}

					if (rank < j[c] + 1)
					{
						rank = j[c] + 1;
						max_value3 = state.kumi_info[13][h];

						q = i[c];
						r = j[c] + 1;

						flag3 = 1;
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] < kumisuu3)
					{
						kumisuu3 = state.kumi_info[13][0];
						rank2 = j[c];

						s = i[c];
						t = j[c] + 1;

						flag4 = 1;
					}
					/*
					if(state.rev==0){
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2<j[c]+1){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=i[c];
							t=j[c]+1;
							
							flag4=1;
						}
					}else{
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2>j[c]+1){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=i[c];
							t=j[c]+1;
							
							flag4=1;
						}
					}
					*/

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] == kumisuu3 && state.kumi_info[13][h] > max_value4)
					{
						kumisuu3 = state.kumi_info[13][0];
						max_value4 = state.kumi_info[13][h];
						//rank2=j[c];

						s = i[c];
						t = j[c] + 1;

						flag4 = 1;
					}

					copy_table[i[c]][j[c] + 1] = 0;
				}

				if (j[c] >= 1 && j[c] <= 11 && copy_table[i[c]][j[c] + 2] == 1 && copy_table[i[c]][j[c] + 1] == 0 && joker_table[i[c]][j[c] + 1] == 0)
				{
					copy_table[i[c]][j[c] + 1] = 2;
					joker_table[i[c]][j[c] + 1] = 2;

					bunkatsu_all2(copy_table);
					count = count + 1;

					if (state.kumi_info[13][h] >= max_value)
					{
						max_value = state.kumi_info[13][h];
						//max_value_count=count;
						f = i[c];
						g = j[c] + 1;
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] <= 1)
					{
						k = i[c];
						l = j[c] + 1;

						flag = 1;
					}

					if (state.kumi_info[13][0] == kumisuu2 && state.kumi_info[13][h] > max_value2)
					{
						kumisuu2 = state.kumi_info[13][0];
						max_value2 = state.kumi_info[13][h];

						o = i[c];
						p = j[c] + 1;

						flag2 = 1;
					}
					if (state.kumi_info[13][0] < kumisuu2)
					{
						kumisuu2 = state.kumi_info[13][0];
						max_value2 = state.kumi_info[13][h];

						o = i[c];
						p = j[c] + 1;

						flag2 = 1;
					}

					if (rank == j[c] + 1 && state.kumi_info[13][h] > max_value3)
					{
						rank = j[c] + 1;
						max_value3 = state.kumi_info[13][h];

						q = i[c];
						r = j[c] + 1;

						flag3 = 1;
					}

					if (rank < j[c] + 1)
					{
						rank = j[c] + 1;
						max_value3 = state.kumi_info[13][h];

						q = i[c];
						r = j[c] + 1;

						flag3 = 1;
					}

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] < kumisuu3)
					{
						kumisuu3 = state.kumi_info[13][0];
						rank2 = j[c];

						s = i[c];
						t = j[c] + 1;

						flag4 = 1;
					}
					/*
					if(state.rev==0){
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2<j[c]+1){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=i[c];
							t=j[c]+1;
							
							flag4=1;
						}
					}else{
						if(state.kumi_info[13][0]-state.kumi_info[13][hh]==kumisuu3&&rank2>j[c]+1){
							kumisuu3=state.kumi_info[13][0];
							rank2=j[c];
							
							s=i[c];
							t=j[c]+1;
							
							flag4=1;
						}
					}
					*/

					if (state.kumi_info[13][0] - state.kumi_info[13][hh] == kumisuu3 && state.kumi_info[13][h] > max_value4)
					{
						kumisuu3 = state.kumi_info[13][0];
						max_value4 = state.kumi_info[13][h];
						//rank2=j[c];

						s = i[c];
						t = j[c] + 1;

						flag4 = 1;
					}

					copy_table[i[c]][j[c] + 1] = 0;
				}
			}

			//jokerの位置の決定、ここから
			/*
				if(state.game_count==4406){
					printf("bunkatsu5.5\n");
				}
			*/

			if (flag == 1 && k != 20 && l != 20)
			{ //<=1により、自分の番になれば勝てると判定できるとき

				copy_table[k][l] = 2;
				bunkatsu_all2(copy_table);
				//printf("k %d l %d\n",k,l);

				kimeta = 1;
				/*
					if(state.game_count==4406){
						printf("bunkatsu5.6\n");
						outputCards(copy_table);
					}
					*/
			}
			/*
				if(kimeta==0&&flag4==1){
					
					copy_table[s][t]=2;
					bunkatsu_all2(copy_table);
					
					kimeta=1;
				}
				*/
			/*
			if(kimeta==0){
				
				if(max_value2>=max_value3){
				
					if(flag2==1&&o!=20&&p!=20&&kumisuu2<kumisuu){

						copy_table[o][p]=2;
						bunkatsu_all2(copy_table);
						//printf("o %d p %d\n",o,p);
						
						kimeta=1;
							
					}
					
					if(flag3==1&&q!=20&&r!=20&&kimeta==0){
						
						copy_table[q][r]=2;
						bunkatsu_all2(copy_table);
						
						if(start_value>state.kumi_info[13][h]){
							copy_table[q][r]=0;
							copy_table[4][1]=2;
							bunkatsu_all2(copy_table);	
						}else{
							
						}
						
						kimeta=1;
						
					}
					
				}else{
					
					if(flag3==1&&q!=20&&r!=20){

						copy_table[q][r]=2;
						bunkatsu_all2(copy_table);
						
						if(start_value>state.kumi_info[13][h]){
							copy_table[q][r]=0;
							copy_table[4][1]=2;
							bunkatsu_all2(copy_table);	
						}else{
					
						}
						
						kimeta=1;
						
					}
					
				}
				
				
			}
			*/
			if (f != 20 && g != 20 && kimeta == 0)
			{

				copy_table[f][g] = 2;
				bunkatsu_all2(copy_table);
				//outputCards(copy_table);
				//printf("f %d g %d\n",f,g);

				kimeta = 1;
			}

			if (kimeta == 0)
			{ //yobi
				copy_table[4][1] = 2;
				bunkatsu_all2(copy_table);
			}
			/*
				if(flag3==1&&q!=20&&r!=20&&kimeta==0){//予備
					
						printf("yobi\n");
					
						copy_table[q][r]=2;
						bunkatsu_all2(copy_table);
						
						if(start_value>state.kumi_info[13][h]){
							copy_table[q][r]=0;
							copy_table[4][1]=2;
							bunkatsu_all2(copy_table);	
							
						}else{
							
						}
						
						kimeta=1;
						
				}
				*/

			//joker_check2(copy_table);

			//printf("flag %d\n",flag);
			//printf("flag2 %d\n",flag2);
			//printf("flag3 %d\n",flag3);
			//printf("q %d r %d\n",q,r);

			//手札評価値を元にした分割は行わない事に。（仮）
			/*
				if(flag==0&&flag2==0&&f!=20&&g!=20&&kimeta==0){

					copy_table[f][g]=2;
					bunkatsu_all2(copy_table);
					//outputCards(copy_table);
					//printf("f %d g %d\n",f,g);
					
					kimeta=1;
					
				}
				*/
			//printf("f %d g %d\n",f,g);

			//

			/*printf("kumi   point\n");
				outputCards(copy_table);
				
				if(qtyOfCards(state.kumi0)>=1)outputCards(state.kumi0);
				if(qtyOfCards(state.kumi1)>=1)outputCards(state.kumi1);
				if(qtyOfCards(state.kumi2)>=1)outputCards(state.kumi2);
				if(qtyOfCards(state.kumi3)>=1)outputCards(state.kumi3);
				if(qtyOfCards(state.kumi4)>=1)outputCards(state.kumi4);
				if(qtyOfCards(state.kumi5)>=1)outputCards(state.kumi5);
				if(qtyOfCards(state.kumi6)>=1)outputCards(state.kumi6);
				if(qtyOfCards(state.kumi7)>=1)outputCards(state.kumi7);
				if(qtyOfCards(state.kumi8)>=1)outputCards(state.kumi8);
				if(qtyOfCards(state.kumi9)>=1)outputCards(state.kumi9);
				if(qtyOfCards(state.kumi10)>=1)outputCards(state.kumi10);
				*/

			//
			/*
				printf("kumisuu2   %d\n",state.kumi_info[13][0]);
				printf("joker[o][p]   joker[%d][%d]\n",o,p);
				printf("max_value2   %d\n",max_value2);
				printf("max_value3   %d\n",max_value3);
			
				joker_check3(copy_table);
				*/
			//outputTable(copy_table);
			//outputTable(joker_table);
		}
	}
	/*
	if(state.game_count==4406){
			printf("bunkatsu5\n");
			outputCards(copy_table);
		}
	*/

	return max_value;
}

void my_change(int out_cards[8][15], int table[8][15], int num_of_change)
{

	int i[13] = {0};
	int j[13] = {0};
	int copy_table[8][15] = {{0}};
	int watasu_card[8][15] = {{0}};
	int temp[8][15] = {{0}};
	int max_value = 0, max_value2 = 0;

	int a = 0, b = 0, c = 0, d = 0, f = 20, g = 20, h = 19, k = 0, l = 0, m = 0, hh = 6, v = 20, w = 20, x = 20, y = 20, hhh = 15; //h=17
	int test = 0, flag = 0;
	int value = 0, value2 = 0;

	int le_5 = 0;
	int kumisuu = 99;
	int rank_max = 0;

	clearTable(out_cards);

	for (a = 0; a < 5; a++)
	{
		for (b = 0; b < 15; b++)
		{
			copy_table[a][b] = table[a][b]; //copytableの作成
		}
	}

	rank_max = get_rank_max(table);

	//2017.09.04//A,2,jokerは渡す候補にあがらないように調整
	for (b = 1; b < 12; b++)
	{
		for (a = 0; a < 4; a++)
		{
			if (copy_table[a][b] == 1)
			{
				//if(a==2&&b==1&&(copy_table[0][1]==1||copy_table[1][1]==1||copy_table[3][1]==1)){//ダイヤの3はペアを構成できる限り、渡さない。datta//&&(copy_table[0][1]==1||copy_table[1][1]==1||copy_table[3][1]==1)||(copy_table[2][2]==1&&copy_table[2][3]==1))
				//	}else{

				i[c] = a;
				j[c] = b;
				c = c + 1;

				//state.submitted_cards_plus_hands[a][b]=1;
				//	}
			}
		}
	}

	//test=qtyOfCards(copy_table);
	d = c; //cが9ならば[0]から[8]の9回繰り返されたということ
		   //	printf("d %d   test %d \n",d,test);
		   //	outputCards(copy_table);
	if (num_of_change == 1)
	{
		/*
	if(copy_table[2][1]==1&&my_lead5_3(temp,copy_table)>0){
		
		for(c=0;c<d;c++){
			
			copy_table[i[c]][j[c]]=0;
			//bunkatsu_pre2(copy_table);
			
			if(copy_table[2][1]==1&&my_lead5_3(temp,copy_table)>0){
				if(my_lead5_2(temp,copy_table)>=le_5){
					le_5=my_lead5_2(temp,copy_table);
					f=i[c];
					g=j[c];
				}
			} 
			
			copy_table[i[c]][j[c]]=1;
		}
		
		if(f!=20&&g!=20&&flag==0)watasu_card[f][g]=1;
		
		copyTable(out_cards,watasu_card);
		if(beEmptyCards(out_cards)==0){
			printf("game_count1   %d\n",state.game_count);
		}

	}
	*/

		if (beEmptyCards(out_cards) == 1)
		{
			for (c = 0; c < d; c++)
			{

				copy_table[i[c]][j[c]] = 0;
				bunkatsu_pre2(copy_table);
				/*
			if(qtyOfCards6(copy_table,2,1)==1){//ダイヤの3の有無の確認
				value=state.kumi_info[13][h]+5;
				value2=state.kumi_info[13][hhh];
				//outputCards(copy_table);
				//printf("value %d\n",value);
			}else{
			value=state.kumi_info[13][h];
			value2=state.kumi_info[13][hhh];
			}
			*/
				/*
			printf("state.kumi_info[13][17]   %d\n",state.kumi_info[13][17]);
			*/

				value = state.kumi_info[13][h];
				value2 = state.kumi_info[13][hhh];

				if (qtyOfCards6(copy_table, 2, 1) == 1)
				{ //ダイヤの3の有無の確認
					value = value + 5;
					//value2=state.kumi_info[13][hhh];
				}

				if (state.game_count == 177 || state.game_count == 3942 || state.game_count == 5239 || state.game_count == 5871 || state.game_count == 5871)
				{
					printf("watasukouho   %d   %d\n", i[c], j[c]);
					//printf("nokori_cards   %d\n",state.kumi_info[13][0]-state.kumi_info[13][50]-d3_flag);
					printf("value   %d\n", value);
					printf("value2   %d\n", value2);
					outputCards(copy_table);
					printf("point   %d\n", state.kumi_info[0][10]);
					outputCards(state.kumi0);
					printf("point   %d\n", state.kumi_info[1][10]);
					outputCards(state.kumi1);
					printf("point   %d\n", state.kumi_info[2][10]);
					outputCards(state.kumi2);
					printf("point   %d\n", state.kumi_info[3][10]);
					outputCards(state.kumi3);
					printf("point   %d\n", state.kumi_info[4][10]);
					outputCards(state.kumi4);
					printf("point   %d\n", state.kumi_info[5][10]);
					outputCards(state.kumi5);
					printf("point   %d\n", state.kumi_info[6][10]);
					outputCards(state.kumi6);
					printf("point   %d\n", state.kumi_info[7][10]);
					outputCards(state.kumi7);
					printf("point   %d\n", state.kumi_info[8][10]);
					outputCards(state.kumi8);
					printf("point   %d\n", state.kumi_info[9][10]);
					outputCards(state.kumi9);
				}
				/*
			if(qtyOfCards6(copy_table,0,1)==1&&copy_table[4][1]==2){
				value=value+1;
				//value2=state.kumi_info[13][hhh];
			}
			*/

				//printf("state.kumi_info[14][0]   %d\n",state.kumi_info[14][0]);

				if (value > max_value)
				{ //&&state.kumi_info[13][0]-state.kumi_info[14][10]<=kumisuu
					max_value = value;
					max_value2 = value2;
					f = i[c];
					g = j[c];
					if (state.kumi_info[13][0] - state.kumi_info[14][10] < kumisuu)
						kumisuu = state.kumi_info[13][0] - state.kumi_info[14][10];
				}

				else if (value == max_value && value2 > max_value2)
				{ //&&value2>=max_value2//&&state.kumi_info[13][0]-state.kumi_info[14][10]<=kumisuu
					max_value = value;
					max_value2 = value2;
					f = i[c];
					g = j[c];
					if (state.kumi_info[13][0] - state.kumi_info[14][10] < kumisuu)
						kumisuu = state.kumi_info[13][0] - state.kumi_info[14][10];
				}

				if (state.kumi_info[13][0] - state.kumi_info[13][hh] <= 1)
				{
					v = i[c];
					w = j[c];
					flag = 1;
					//printf("///////////////////////////////////////////////////////////////////////////////////\n");
				}

				copy_table[i[c]][j[c]] = 1;
			}

			if (f != 20 && g != 20 && flag == 0)
			{
				watasu_card[f][g] = 1;
				//state.submitted_cards_plus_hands[f][g]=0;
			}
			if (v != 20 && w != 20 && flag == 1)
			{
				watasu_card[v][w] = 1; //ほとんど使われない
									   //state.submitted_cards_plus_hands[v][w]=0;
			}

			//printf("f   %d   g   %d\n",f,g);
			copyTable(out_cards, watasu_card);
		}
	}

	if (num_of_change == 2)
	{
		/*
	if(copy_table[2][1]==1&&my_lead5_3(temp,copy_table)>0){
		
		for(k=0;k<d;k++){
			for(c=0;c<d;c++){
			
				if(k<c){
					copy_table[i[c]][j[c]]=0;
					copy_table[i[k]][j[k]]=0;
					
					//bunkatsu_pre2(copy_table);
					
					if(copy_table[2][1]==1&&my_lead5_3(temp,copy_table)>0){
						if(my_lead5_2(temp,copy_table)>=le_5){
							le_5=my_lead5_2(temp,copy_table);
							f=i[c];
							g=j[c];
							l=i[k];
							m=j[k];
						}
					} 
					
					
					copy_table[i[c]][j[c]]=1;
					copy_table[i[k]][j[k]]=1;
				}
		
			}
		}
		
		
		if(f!=20&&g!=20&&l!=20&&m!=20&&flag==0){
			watasu_card[f][g]=1;
			watasu_card[l][m]=1;
		}

		copyTable(out_cards,watasu_card);
		if(beEmptyCards(out_cards)==0){
			printf("game_count2   %d\n",state.game_count);
		}
		
	}
	*/

		if (beEmptyCards(out_cards) == 1)
		{

			for (k = 0; k < d; k++)
			{
				for (c = 0; c < d; c++)
				{

					//printf("i[c] %d   j[c] %d\n",i[c],j[c]);

					if (k < c)
					{
						copy_table[i[c]][j[c]] = 0;
						copy_table[i[k]][j[k]] = 0;

						bunkatsu_pre2(copy_table);
						/*
					if(qtyOfCards6(copy_table,2,1)==1){//ダイヤの3の有無の確認
						value=state.kumi_info[13][h]+5;
						value2=state.kumi_info[13][hhh];
						//printf("value %d\n",value);
					}else{
						value=state.kumi_info[13][h];
						value2=state.kumi_info[13][hhh];
					}
					*/
						value = state.kumi_info[13][h];
						value2 = state.kumi_info[13][hhh];

						if (qtyOfCards6(copy_table, 2, 1) == 1)
						{ //ダイヤの3の有無の確認
							value = value + 5;
							//value2=state.kumi_info[13][hhh];
						}
						/*
					if(qtyOfCards6(copy_table,0,1)==1&&copy_table[4][1]==2){
						value=value+1;
						//value2=state.kumi_info[13][hhh];
					}
					*/
						//printf("state.kumi_info[14][0]   %d\n",state.kumi_info[14][0]);

						if (value > max_value)
						{ //&&state.kumi_info[13][0]-state.kumi_info[14][10]<=kumisuu
							max_value = value;
							max_value2 = value2;
							f = i[c];
							g = j[c];
							l = i[k];
							m = j[k];
							if (state.kumi_info[13][0] - state.kumi_info[14][10] < kumisuu)
								kumisuu = state.kumi_info[13][0] - state.kumi_info[14][10];
						}

						else if (value == max_value && value2 > max_value2)
						{ //&&value2>=max_value2//&&state.kumi_info[13][0]-state.kumi_info[14][10]<=kumisuu
							max_value = value;
							max_value2 = value2;
							f = i[c];
							g = j[c];
							l = i[k];
							m = j[k];
							if (state.kumi_info[13][0] - state.kumi_info[14][10] < kumisuu)
								kumisuu = state.kumi_info[13][0] - state.kumi_info[14][10];
						}

						if (state.kumi_info[13][0] - state.kumi_info[13][hh] <= 1)
						{
							v = i[c];
							w = j[c];
							x = i[k];
							y = j[k];
							flag = 1;

							//printf("2/////////////////////////////////////////////////////////////////////////////\n");
						}

						//printf("test3\n");
						copy_table[i[c]][j[c]] = 1;
						copy_table[i[k]][j[k]] = 1;
					}
				}
			}

			if (f != 20 && g != 20 && l != 20 && m != 20 && flag == 0)
			{
				watasu_card[f][g] = 1;
				watasu_card[l][m] = 1;
				//state.submitted_cards_plus_hands[f][g]=0;
				//state.submitted_cards_plus_hands[l][m]=0;
			}
			if (v != 20 && w != 20 && flag == 1)
			{
				watasu_card[v][w] = 1; //ほとんど使われない
				watasu_card[x][y] = 1;
				//state.submitted_cards_plus_hands[v][w]=0;
				//state.submitted_cards_plus_hands[x][y]=0;
			}

			//printf("test4\n");

			copyTable(out_cards, watasu_card);
		}
	}
	//clearTable(state.submitted_cards_plus_hands);
	//printf("test5\n");
}

void my_change2(int out_cards[8][15], int table[8][15], int num_of_change)
{

	int i[13] = {0};
	int j[13] = {0};
	int copy_table[8][15] = {{0}};
	int watasu_card[8][15] = {{0}};
	int temp[8][15] = {{0}};
	int max_value = 0, max_value2 = 0;

	int a = 0, b = 0, c = 0, d = 0, f = 20, g = 20, h = 4, k = 0, l = 0, m = 0, hh = 6, v = 20, w = 20, x = 20, y = 20, hhh = 19; //h=17
	int test = 0, flag = 0;
	int value = 0, value2 = 0;

	int le_5 = 0;
	int kumisuu = 99;
	int rank_max = 0;

	clearTable(out_cards);

	for (a = 0; a < 5; a++)
	{
		for (b = 0; b < 15; b++)
		{
			copy_table[a][b] = table[a][b]; //copytableの作成
		}
	}

	rank_max = get_rank_max(table);

	//2017.09.04//A,2,jokerは渡す候補にあがらないように調整
	for (b = 1; b < 12; b++)
	{
		for (a = 0; a < 4; a++)
		{
			if (copy_table[a][b] == 1)
			{
				//if(a==2&&b==1&&(copy_table[0][1]==1||copy_table[1][1]==1||copy_table[3][1]==1)){//ダイヤの3はペアを構成できる限り、渡さない。datta//&&(copy_table[0][1]==1||copy_table[1][1]==1||copy_table[3][1]==1)||(copy_table[2][2]==1&&copy_table[2][3]==1))
				//	}else{

				i[c] = a;
				j[c] = b;
				c = c + 1;

				//state.submitted_cards_plus_hands[a][b]=1;
				//	}
			}
		}
	}

	//test=qtyOfCards(copy_table);
	d = c; //cが9ならば[0]から[8]の9回繰り返されたということ
		   //	printf("d %d   test %d \n",d,test);
		   //	outputCards(copy_table);
	if (num_of_change == 1)
	{

		if (beEmptyCards(out_cards) == 1)
		{
			for (c = 0; c < d; c++)
			{

				copy_table[i[c]][j[c]] = 0;
				bunkatsu_pre3(copy_table);

				value = state.kumi_info[13][h];
				value2 = state.kumi_info[13][hhh];

				if (qtyOfCards6(copy_table, 2, 1) == 1)
				{ //ダイヤの3の有無の確認
					value = value + 5;
					//value2=state.kumi_info[13][hhh];
				}

				if (state.game_count == 177 || state.game_count == 3942 || state.game_count == 5239 || state.game_count == 5871 || state.game_count == 5871)
				{
					printf("watasukouho   %d   %d\n", i[c], j[c]);
					//printf("nokori_cards   %d\n",state.kumi_info[13][0]-state.kumi_info[13][50]-d3_flag);
					printf("value   %d\n", value);
					printf("value2   %d\n", value2);
					outputCards(copy_table);
					printf("point   %d\n", state.kumi_info[0][10]);
					outputCards(state.kumi0);
					printf("point   %d\n", state.kumi_info[1][10]);
					outputCards(state.kumi1);
					printf("point   %d\n", state.kumi_info[2][10]);
					outputCards(state.kumi2);
					printf("point   %d\n", state.kumi_info[3][10]);
					outputCards(state.kumi3);
					printf("point   %d\n", state.kumi_info[4][10]);
					outputCards(state.kumi4);
					printf("point   %d\n", state.kumi_info[5][10]);
					outputCards(state.kumi5);
					printf("point   %d\n", state.kumi_info[6][10]);
					outputCards(state.kumi6);
					printf("point   %d\n", state.kumi_info[7][10]);
					outputCards(state.kumi7);
					printf("point   %d\n", state.kumi_info[8][10]);
					outputCards(state.kumi8);
					printf("point   %d\n", state.kumi_info[9][10]);
					outputCards(state.kumi9);
				}

				if (value > max_value)
				{ //&&state.kumi_info[13][0]-state.kumi_info[14][10]<=kumisuu
					max_value = value;
					max_value2 = value2;
					f = i[c];
					g = j[c];
					if (state.kumi_info[13][0] - state.kumi_info[14][10] < kumisuu)
						kumisuu = state.kumi_info[13][0] - state.kumi_info[14][10];
				}

				else if (value == max_value && value2 > max_value2)
				{ //&&value2>=max_value2//&&state.kumi_info[13][0]-state.kumi_info[14][10]<=kumisuu
					max_value = value;
					max_value2 = value2;
					f = i[c];
					g = j[c];
					if (state.kumi_info[13][0] - state.kumi_info[14][10] < kumisuu)
						kumisuu = state.kumi_info[13][0] - state.kumi_info[14][10];
				}

				if (state.kumi_info[13][0] - state.kumi_info[13][hh] <= 1)
				{
					v = i[c];
					w = j[c];
					flag = 1;
					//printf("///////////////////////////////////////////////////////////////////////////////////\n");
				}

				copy_table[i[c]][j[c]] = 1;
			}

			if (f != 20 && g != 20 && flag == 0)
			{
				watasu_card[f][g] = 1;
				//state.submitted_cards_plus_hands[f][g]=0;
			}
			if (v != 20 && w != 20 && flag == 1)
			{
				watasu_card[v][w] = 1; //ほとんど使われない
									   //state.submitted_cards_plus_hands[v][w]=0;
			}

			copyTable(out_cards, watasu_card);
		}
	}

	if (num_of_change == 2)
	{

		if (beEmptyCards(out_cards) == 1)
		{

			for (k = 0; k < d; k++)
			{
				for (c = 0; c < d; c++)
				{

					if (k < c)
					{
						copy_table[i[c]][j[c]] = 0;
						copy_table[i[k]][j[k]] = 0;

						bunkatsu_pre3(copy_table);

						value = state.kumi_info[13][h];
						value2 = state.kumi_info[13][hhh];

						if (qtyOfCards6(copy_table, 2, 1) == 1)
						{ //ダイヤの3の有無の確認
							value = value + 5;
							//value2=state.kumi_info[13][hhh];
						}

						if (value > max_value)
						{ //&&state.kumi_info[13][0]-state.kumi_info[14][10]<=kumisuu
							max_value = value;
							max_value2 = value2;
							f = i[c];
							g = j[c];
							l = i[k];
							m = j[k];
							//if(state.kumi_info[13][0]-state.kumi_info[14][10]<kumisuu)kumisuu=state.kumi_info[13][0]-state.kumi_info[14][10];
						}

						else if (value == max_value && value2 > max_value2)
						{ //&&value2>=max_value2//&&state.kumi_info[13][0]-state.kumi_info[14][10]<=kumisuu
							max_value = value;
							max_value2 = value2;
							f = i[c];
							g = j[c];
							l = i[k];
							m = j[k];
							//if(state.kumi_info[13][0]-state.kumi_info[14][10]<kumisuu)kumisuu=state.kumi_info[13][0]-state.kumi_info[14][10];
						}

						if (state.kumi_info[13][0] - state.kumi_info[13][hh] <= 1)
						{
							v = i[c];
							w = j[c];
							x = i[k];
							y = j[k];
							flag = 1;
						}

						//printf("test3\n");
						copy_table[i[c]][j[c]] = 1;
						copy_table[i[k]][j[k]] = 1;
					}
				}
			}

			if (f != 20 && g != 20 && l != 20 && m != 20 && flag == 0)
			{
				watasu_card[f][g] = 1;
				watasu_card[l][m] = 1;
				//state.submitted_cards_plus_hands[f][g]=0;
				//state.submitted_cards_plus_hands[l][m]=0;
			}
			if (v != 20 && w != 20 && flag == 1)
			{
				watasu_card[v][w] = 1; //ほとんど使われない
				watasu_card[x][y] = 1;
				//state.submitted_cards_plus_hands[v][w]=0;
				//state.submitted_cards_plus_hands[x][y]=0;
			}
			copyTable(out_cards, watasu_card);
		}
	}
}

void my_change3(int out_cards[8][15], int table[8][15], int num_of_change)
{

	int i[13] = {0};
	int j[13] = {0};
	int copy_table[8][15] = {{0}};
	int watasu_card[8][15] = {{0}};
	int temp[8][15] = {{0}};
	int max_value = 0, max_value2 = 0;

	int a = 0, b = 0, c = 0, d = 0, f = 20, g = 20, h = 4, k = 0, l = 0, m = 0, hh = 6, v = 20, w = 20, x = 20, y = 20, hhh = 15; //h=17
	int test = 0, flag = 0;
	int value = 0, value2 = 0;

	int le_5 = 0;
	int kumisuu = 99;
	int rank_max = 0;
	int d3_flag = 0;
	int sum_gm = 99;

	clearTable(out_cards);

	for (a = 0; a < 5; a++)
	{
		for (b = 0; b < 15; b++)
		{
			copy_table[a][b] = table[a][b]; //copytableの作成
		}
	}

	rank_max = get_rank_max(table);

	//2017.09.04//A,2,jokerは渡す候補にあがらないように調整
	for (b = 1; b < 13; b++)
	{
		for (a = 0; a < 4; a++)
		{
			if (copy_table[a][b] == 1)
			{
				//if(a==2&&b==1&&(copy_table[0][1]==1||copy_table[1][1]==1||copy_table[3][1]==1)){//ダイヤの3はペアを構成できる限り、渡さない。datta//&&(copy_table[0][1]==1||copy_table[1][1]==1||copy_table[3][1]==1)||(copy_table[2][2]==1&&copy_table[2][3]==1))
				//	}else{

				i[c] = a;
				j[c] = b;
				c = c + 1;

				//state.submitted_cards_plus_hands[a][b]=1;
				//	}
			}
		}
	}

	//test=qtyOfCards(copy_table);
	d = c; //cが9ならば[0]から[8]の9回繰り返されたということ
		   //	printf("d %d   test %d \n",d,test);
		   //	outputCards(copy_table);
	if (num_of_change == 1)
	{

		if (beEmptyCards(out_cards) == 1)
		{
			for (c = 0; c < d; c++)
			{

				copy_table[i[c]][j[c]] = 0;
				bunkatsu_pre3(copy_table);

				value = state.kumi_info[13][h];
				value2 = state.kumi_info[13][hhh];

				if (qtyOfCards6(copy_table, 2, 1) == 1)
				{ //ダイヤの3の有無の確認
					value = value + 5;
					d3_flag = 1;
				}
				else
				{
					d3_flag = 0;
				}

				if (state.game_count == 63 || state.game_count == 191 || state.game_count == 2806)
				{
					printf("watasukouho   %d   %d\n", i[c], j[c]);
					printf("value   %d\n", value);
					//printf("value2   %d\n",value2);
					printf("nokori_cards   %d\n", state.kumi_info[13][0] - state.kumi_info[13][50] - d3_flag);
					outputCards(copy_table);
					printf("point   %d\n", state.kumi_info[0][10]);
					outputCards(state.kumi0);
					printf("point   %d\n", state.kumi_info[1][10]);
					outputCards(state.kumi1);
					printf("point   %d\n", state.kumi_info[2][10]);
					outputCards(state.kumi2);
					printf("point   %d\n", state.kumi_info[3][10]);
					outputCards(state.kumi3);
					printf("point   %d\n", state.kumi_info[4][10]);
					outputCards(state.kumi4);
					printf("point   %d\n", state.kumi_info[5][10]);
					outputCards(state.kumi5);
					printf("point   %d\n", state.kumi_info[6][10]);
					outputCards(state.kumi6);
					printf("point   %d\n", state.kumi_info[7][10]);
					outputCards(state.kumi7);
					printf("point   %d\n", state.kumi_info[8][10]);
					outputCards(state.kumi8);
					printf("point   %d\n", state.kumi_info[9][10]);
					outputCards(state.kumi9);
				}
				/*
			if(value>max_value){//&&state.kumi_info[13][0]-state.kumi_info[14][10]<=kumisuu
						max_value=value;
						max_value2=value2;
						f=i[c];
						g=j[c];
						if(state.kumi_info[13][0]-state.kumi_info[14][10]<kumisuu)kumisuu=state.kumi_info[13][0]-state.kumi_info[14][10];
			} 
			
			else if(value==max_value&&value2>max_value2){//&&value2>=max_value2//&&state.kumi_info[13][0]-state.kumi_info[14][10]<=kumisuu
						max_value=value;
						max_value2=value2;
						f=i[c];
						g=j[c];
						if(state.kumi_info[13][0]-state.kumi_info[14][10]<kumisuu)kumisuu=state.kumi_info[13][0]-state.kumi_info[14][10];
			}
			*/

				if (state.kumi_info[13][0] - state.kumi_info[13][50] - d3_flag == kumisuu && state.kumi_info[13][h] > value)
				{
					f = i[c];
					g = j[c];
					kumisuu = state.kumi_info[13][0] - state.kumi_info[13][50] - d3_flag;
					value = state.kumi_info[13][h];
				}

				if (state.kumi_info[13][0] - state.kumi_info[13][50] - d3_flag < kumisuu)
				{
					f = i[c];
					g = j[c];
					kumisuu = state.kumi_info[13][0] - state.kumi_info[13][50] - d3_flag;
					value = state.kumi_info[13][h];
				}

				if (state.kumi_info[13][0] - state.kumi_info[13][hh] <= 1)
				{
					v = i[c];
					w = j[c];
					flag = 1;
				}

				copy_table[i[c]][j[c]] = 1;
			}

			if (f != 20 && g != 20 && flag == 0)
			{
				watasu_card[f][g] = 1;
				//state.submitted_cards_plus_hands[f][g]=0;
			}
			if (v != 20 && w != 20 && flag == 1)
			{
				watasu_card[v][w] = 1; //ほとんど使われない
									   //state.submitted_cards_plus_hands[v][w]=0;
			}

			//printf("f   %d   g   %d\n",f,g);
			copyTable(out_cards, watasu_card);
		}
	}

	if (num_of_change == 2)
	{

		if (beEmptyCards(out_cards) == 1)
		{

			for (k = 0; k < d; k++)
			{
				for (c = 0; c < d; c++)
				{

					if (k < c)
					{
						copy_table[i[k]][j[k]] = 0;
						copy_table[i[c]][j[c]] = 0;

						bunkatsu_pre3(copy_table);

						value = state.kumi_info[13][h];
						value2 = state.kumi_info[13][hhh];

						if (qtyOfCards6(copy_table, 2, 1) == 1)
						{ //ダイヤの3の有無の確認
							value = value + 5;
							d3_flag = 1;
						}
						else
						{
							d3_flag = 0;
						}
						/*
					if(state.game_count>=9998){
						printf("watasukouho   %d   %d\n",i[k],j[k]);
						printf("watasukouho   %d   %d\n",i[c],j[c]);
						printf("nokori_cards   %d\n",state.kumi_info[13][0]-state.kumi_info[13][50]-d3_flag);
						outputCards(copy_table);
						outputCards(state.kumi0);
						outputCards(state.kumi1);
						outputCards(state.kumi2);
						outputCards(state.kumi3);
						outputCards(state.kumi4);
						outputCards(state.kumi5);
						outputCards(state.kumi6);
						outputCards(state.kumi7);
						outputCards(state.kumi8);
						outputCards(state.kumi9);
					}
					*/
						/*
					if(value>max_value){//&&state.kumi_info[13][0]-state.kumi_info[14][10]<=kumisuu
						max_value=value;
						max_value2=value2;
						f=i[c];
						g=j[c];
						l=i[k];
						m=j[k];
						if(state.kumi_info[13][0]-state.kumi_info[14][10]<kumisuu)kumisuu=state.kumi_info[13][0]-state.kumi_info[14][10];
					}
					
					else if(value==max_value&&value2>max_value2){//&&value2>=max_value2//&&state.kumi_info[13][0]-state.kumi_info[14][10]<=kumisuu
						max_value=value;
						max_value2=value2;
						f=i[c];
						g=j[c];
						l=i[k];
						m=j[k];
						if(state.kumi_info[13][0]-state.kumi_info[14][10]<kumisuu)kumisuu=state.kumi_info[13][0]-state.kumi_info[14][10];
					}
					*/

						if (state.kumi_info[13][0] - state.kumi_info[13][50] - d3_flag == kumisuu && state.kumi_info[13][h] > value)
						{
							f = i[c];
							g = j[c];
							l = i[k];
							m = j[k];
							//sum_gm=g+m;
							kumisuu = state.kumi_info[13][0] - state.kumi_info[13][50] - d3_flag;
						}

						if (state.kumi_info[13][0] - state.kumi_info[13][50] - d3_flag < kumisuu)
						{
							f = i[c];
							g = j[c];
							l = i[k];
							m = j[k];
							//sum_gm=g+m;
							kumisuu = state.kumi_info[13][0] - state.kumi_info[13][50] - d3_flag;
						}

						if (state.kumi_info[13][0] - state.kumi_info[13][hh] <= 1)
						{
							v = i[c];
							w = j[c];
							x = i[k];
							y = j[k];
							flag = 1;
						}

						//printf("test3\n");
						copy_table[i[c]][j[c]] = 1;
						copy_table[i[k]][j[k]] = 1;
					}
				}
			}

			if (f != 20 && g != 20 && l != 20 && m != 20 && flag == 0)
			{
				watasu_card[f][g] = 1;
				watasu_card[l][m] = 1;
				//state.submitted_cards_plus_hands[f][g]=0;
				//state.submitted_cards_plus_hands[l][m]=0;
			}
			if (v != 20 && w != 20 && flag == 1)
			{
				watasu_card[v][w] = 1; //ほとんど使われない
				watasu_card[x][y] = 1;
				//state.submitted_cards_plus_hands[v][w]=0;
				//state.submitted_cards_plus_hands[x][y]=0;
			}

			//printf("test4\n");

			copyTable(out_cards, watasu_card);
		}
	}
	//clearTable(state.submitted_cards_plus_hands);
	//printf("test5\n");
}

///////////////////////////////////////////////////////////////////////////////////////]
///////////////ここからVV8以外////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------
//--------------------------------ここから自作の関数--------------------------------
//----------------------------------------------------------------------------------

int setmake(int out_cards[][15], int my_cards[8][15], int joker_flag)
{
	//できるだけ組数が少なくできるよう階段を作り、階段を格納したものと、合計の組数を返す
	//out_cards[0]~[3]には階段で出せる場合は右端の場所に階段の枚数が入り、階段以外のカードは1が入る
	//out_cards[4]には階段に使用していないカードの枚数が入る
	int tgt_cards[8][15]; //テーブル
	int i, j, k;
	int count = 0;
	//out_cards[4][14]=0;
	clearTable(out_cards);
	clearTable(tgt_cards);
	for (i = 0; i < 4; i++)
	{
		for (j = 1; j <= 13; j++)
		{
			tgt_cards[5][j] += my_cards[i][j]; //各数字の枚数を記録
		}
	}
	for (i = 0; i < 4; i++)
	{ //各スート毎に走査し
		for (j = 1; j <= 12; j++)
		{ //Aまで順番にみて
			if (my_cards[i][j] == 1)
			{			 //カードがあるとき
				count++; //カウンタを進め
			}
			else
			{
				count = 0; //カードがないときリセットする
			}
			if (count > 2)
			{ //3枚以上のときその枚数をテーブルに格納
				tgt_cards[i][j] = count;
			}
			else
			{
				tgt_cards[i][j] = 0; //その他は0にする
			}
		}
		count = 0;
	}
	count = 0;
	//階段ができた場合
	//例えばスペード5～7の場合tgt_cardsの[0][5]は3,[0][3],[0][4]は0となっている
	//スペード5～8の場合tgt_cardsの[0][6]は4,[0][5]は3,[0][3],[0][4]は0となっている
	for (i = 0; i < 4; i++)
	{
		for (j = 13; j > 0; j--)
		{
			if (tgt_cards[i][j] >= 3)
			{ //階段を発見したとき
				count = 0;
				for (k = 0; k < tgt_cards[i][j]; k++)
				{ //その階段の数字をみて
					if (tgt_cards[5][j - k] >= 2 && j != 6)
					{			 //8のカード以外で階段以外にもその数字があれば
						count++; //カウンタを進める
					}
				}
				if (j >= 6 && j - tgt_cards[i][j] < 6)
				{ //8が含まれている場合
					if (count + 1 == tgt_cards[i][j])
					{									  //8のカード以外の階段の数字がすべてペアとなる場合、階段を崩す
						out_cards[i][j] = my_cards[i][j]; //カードを残す
					}
					else
					{
						out_cards[i][j] = tgt_cards[i][j]; //階段を残す(作る)
						for (k = 0; k < tgt_cards[i][j]; k++)
						{
							tgt_cards[5][j - k] -= 1; //その階段の数字の枚数を減らす
						}
						j -= k; //階段の左端の1つ左に移動
					}
				}
				else
				{ //8が含まれてない場合
					if (count == tgt_cards[i][j])
					{									  //階段の数字がすべてペアとなる場合、階段を崩す
						out_cards[i][j] = my_cards[i][j]; //カードを残す
					}
					else
					{
						out_cards[i][j] = tgt_cards[i][j]; //階段を残す(作る)
						for (k = 0; k < tgt_cards[i][j]; k++)
						{
							tgt_cards[5][j - k] -= 1; //その階段の数字の枚数を減らす
						}
						j -= k; //階段の左端の1つ左に移動
					}
				}
			} //階段を発見したときの判定ここまで
			else
			{									  //階段でない
				out_cards[i][j] = my_cards[i][j]; //カードを残す
			}
		}
	}
	count = 0;
	//out_cardsに階段でない部分に1,階段の部分の右端にその枚数の数字が入る
	//スペード6～8の場合out_cardsの[0][6]は3,[0][4],[0][5]は0となっている
	//スペード6～9の場合out_cardsの[0][7]は4,[0][4],[0][5],[0][6]は0となっている

	//ジョーカーがあるとき,階段を捜し,判定
	clearCards(tgt_cards); //カード部分リセット
	k = 0;
	if (joker_flag == 1)
	{ //jokerがあるとき
		for (i = 0; i < 4; i++)
		{ //各スート毎に走査し
			for (j = 1; j <= 12; j++)
			{ //Aまで順番にみて
				if (my_cards[i][j] == 1 && (tgt_cards[5][j] == 1 || j == 6))
				{			 //カードがあり、単体または8のカードのとき
					count++; //カウンタを進める
				}
				else if (k == 0 && count != 0 && my_cards[i][j] != 1)
				{			 //ジョーカー未使用でカードがないとき
					count++; //カウンタを進める
					k = j;	 //ジョーカーの場所を記録
				}
				else
				{ //探索後、階段ができるか判定
					if (count > 2)
					{								 //3枚以上のとき階段を作り,ループから抜ける
						out_cards[i][j - 1] = count; //右端に枚数格納
						if (k != j - 1)
						{							  //ジョーカーの位置=階段の右端(2に近い方)でないとき
							tgt_cards[5][j - 1] -= 1; //その階段の数字の枚数を減らす(下の判定では右端を調べないため)
						}
						for (; count > 1; count--)
						{
							out_cards[i][j - count] = 0;
							if (k != j - count)
							{
								tgt_cards[5][j - count] -= 1; //その階段の数字の枚数を減らす
							}
						}
						out_cards[4][14] = 2;
						i = 4;
						j = 13;
					}
					else
					{ //2枚以下ならリセット
						count = 0;
						k = 0;
					}
				}
				if (j == 12)
				{ //Aまで調べて端についたとき
					if (count > 2)
					{ //以降、ループ内の階段判定と同じ処理
						out_cards[i][j] = count;
						if (k != j)
						{
							tgt_cards[5][j] -= 1;
						}
						for (count = count - 1; count > 0; count--)
						{
							out_cards[i][j - count] = 0;
							if (k != j - count)
							{
								tgt_cards[5][j - count] -= 1;
							}
						}
						out_cards[4][14] = 2;
						i = 4;
						j = 13;
					}
					else
					{
						count = 0;
						k = 0;
					}
				}
			} //数字ループここまで
		}	  //スートループここまで
	}
	//スペード6,8とジョーカーで作った場合out_cardsの[0][6]は3,[0][4],[0][5]は0,k=5となっている
	if (out_cards[4][14] != 2)
	{ //ジョーカーで階段を作らない場合、ジョーカーを格納
		out_cards[4][14] = state.joker;
	}
	//組数を計算
	count = 0;
	for (i = 0; i < 4; i++)
	{ //各スート毎に走査し
		for (j = 1; j <= 13; j++)
		{ //順番にみて
			if (out_cards[i][j] > 2)
			{			 //階段があれば
				count++; //カウント
			}
		}
	}
	for (j = 1; j <= 13; j++)
	{
		out_cards[4][j] = 0;
		for (i = 0; i < 4; i++)
		{
			if (out_cards[i][j] == 1)
			{
				out_cards[4][j] += 1; //各数字の枚数を記録
			}
		}
	}
	for (j = 1; j <= 13; j++)
	{
		if (out_cards[4][j] > 0)
		{			 //階段以外でカードがあれば
			count++; //カウント
		}
	}
	return count;
}

int kaidanhand(int select_cards[][15], int search[8][15], int own_cards[8][15])
{
	//階段をsearchから探し、結果をselect_cardsに格納する
	clearTable(select_cards);
	int i, j, k;
	for (j = 3; j <= 13; j++)
	{
		for (i = 0; i < 4; i++)
		{
			if (search[i][j] >= 3)
			{
				for (k = 0; k < search[i][j]; k++)
				{
					if (own_cards[i][j - k] == 0)
					{
						select_cards[i][j - k] = 2;
					}
					else
					{
						select_cards[i][j - k] = 1;
					}
				}
				return j; //カードの強さを返す
			}
		}
	}
	return 0; //なければ0を返す
}

int grouphand(int select_cards[][15], int search[8][15], int n)
{
	//指定の枚数の枚数組を探し、結果をselect_cardsに格納する
	clearTable(select_cards);
	int i, j;
	if (state.rev == 0)
	{
		for (j = 1; j <= 13; j++)
		{
			if (n != 0)
			{
				//枚数が指定されている場合
				if (search[4][j] == n)
				{
					for (i = 0; i <= 3; i++)
					{
						if (search[i][j] == 1)
						{
							select_cards[i][j] = 1;
						}
					}
					return j; //カードの強さを返す
				}
			}
			else
			{
				//枚数が指定されていない場合
				if (search[4][j] > 0)
				{
					for (i = 0; i <= 3; i++)
					{
						if (search[i][j] == 1)
						{
							select_cards[i][j] = 1;
						}
					}
					return j; //カードの強さを返す
				}
			}
		}
	}
	else
	{
		//革命時も同じように選ぶ
		for (j = 13; j >= 1; j--)
		{
			if (n != 0)
			{
				if (search[4][j] == n)
				{
					for (i = 0; i <= 3; i++)
					{
						if (search[i][j] == 1)
						{
							select_cards[i][j] = 1;
						}
					}
					return j; //カードの強さを返す
				}
			}
			else
			{
				if (search[4][j] > 0)
				{
					for (i = 0; i <= 3; i++)
					{
						if (search[i][j] == 1)
						{
							select_cards[i][j] = 1;
						}
					}
					return j; //カードの強さを返す
				}
			}
		}
	}
	return 0; //カードがなければ0を返す
}

void kou_lead(int select_cards[][15], int own_cards[8][15], int max, int used_cards[8][15], int tes[3][4][3])
{
	//新しくカードを出すときの思考
	int i, j, k, l, m;
	int search[8][15];
	clearTable(search);
	int pattern[13][7] = {{0}};
	pat_make(pattern, own_cards, max, used_cards, state.joker, tes);
	if (own_cards[4][1] == 2)
	{									   //ジョーカーがある場合
		int target_pattern[13][7] = {{0}}; //手札組の候補を入れる
		int count[15] = {0};			   //カードの枚数 or ジョーカーペアを判断するか(しない場合-1)
		for (i = 0; i < 4; i++)
		{
			for (j = 1; j <= 13; j++)
			{
				if (state.rev == 0 && max <= j)
				{
					count[j] = -1;
				}
				else if (state.rev == 1 && max >= j)
				{
					count[j] = -1;
				}
				else if (own_cards[i][j] == 1)
				{
					count[j]++;
				}
			}
		}
		for (i = 0; i < 4; i++)
		{
			for (j = 0; j <= 14; j++)
			{
				k = 0;
				if (own_cards[i][j] == 0)
				{ //ジョーカーを代用するか判定
					if (j <= 11)
					{
						if (own_cards[i][j + 1] == 1 && own_cards[i][j + 2] == 1)
						{ //右2箇所にカードがある
							k = 1;
						}
					}
					if (j >= 3)
					{
						if (own_cards[i][j - 1] == 1 && own_cards[i][j - 2] == 1)
						{ //左2箇所にカードがある
							k = 1;
						}
					}
					if (k == 0 && own_cards[i][j - 1] == 1 && own_cards[i][j + 1] == 1)
					{ //左右にカードがある
						k = 1;
					}
					if (k == 0 && count[j] >= 1)
					{ //同じ数字のカードが1枚以上(1回のみ)
						k = 2;
						count[j] = -1;
					}
				} //ジョーカーを代用するか判定ここまで
				if (k >= 1)
				{ //判定する場合,判定前のpatternと比較して良いかどうか判断
					//変換
					own_cards[i][j] = 1;
					pat_make(target_pattern, own_cards, max, used_cards, 0, tes);
					own_cards[i][j] = 0;
					//条件式
					if (target_pattern[11][3] >= STRONG)
					{ //あがり手の場合
						if (target_pattern[11][3] > pattern[11][3])
						{ //2minが高い
							k = 3;
						}
					}
					else if (target_pattern[11][0] < pattern[11][0] && k == 1)
					{ //組が減る
						k = 3;
					}
					else if (target_pattern[11][0] == pattern[11][0] && target_pattern[11][4] < pattern[11][4] && k == 1)
					{ //組数同じでzが小さくなる
						k = 3;
					}
				}
				for (l = 0; l < 13; l++)
				{
					for (m = 0; m < 7; m++)
					{
						if (k == 3)
						{
							pattern[l][m] = target_pattern[l][m];
						}
						target_pattern[l][m] = 0;
					}
				}
				if (pattern[11][3] >= 100)
				{ //確実に勝てる場合は判定終了
					i = 4;
					j = 15;
				}
			} //jループ
		}	  //iループ
	}		  //ジョーカーの使用位置決定ここまで
	for (i = 0; i < pattern[11][0]; i++)
	{ //革命手があるかどうか
		if (pattern[i][1] >= 5 || (pattern[i][1] == 4 && (pattern[i][3] == 0 || pattern[i][3] == 14)))
		{
			pattern[11][1] = 1;
		}
	}
	if (pattern[11][2] == -1)
	{
		//もし予想外の動作が起きた場合,defaultの関数を使う
		if (state.rev == 0)
		{
			lead(select_cards, own_cards); //通常時の提出用
		}
		else
		{
			leadRev(select_cards, own_cards); //革命時の提出用
		}
	}
	else
	{
		if (pattern[11][1] == 0)
		{ //革命手がないとき,またはあがり判定の時は通常の判定に移る
			//あがり手札と判定していない場合
			j = 0;
			if (pattern[11][3] < STRONG && pattern[11][0] <= 4)
			{
				for (i = 0; i < pattern[11][0]; i++)
				{
					if (pattern[i][1] >= 3)
					{
						j++;
					}
				}
			}
			if (j != 0 && pattern[11][0] - j <= 3)
			{ //組の数-階段の組の数<=3であれば階段優先
				for (i = 0; i < pattern[11][0]; i++)
				{
					if (pattern[i][1] >= 3)
					{
						if (state.rev == 0 && pattern[i][0] + 2 < max)
						{
							pattern[i][6] = 90;
						}
						else if (state.rev == 1 && pattern[i][0] - 2 > max)
						{
							pattern[i][6] = 90;
						}
					}
				}
			}
			k = 2; //通常判定に移る
		}
		else if (pattern[11][0] <= 2)
		{
			k = 1; //組が2以下のときは革命を起こす
		}
		else
		{
			j = 1;
			for (i = 0; i < pattern[11][0]; i++)
			{
				if (pattern[i][4] <= 100 && pattern[i][3] != 14 && pattern[i][1] <= 2 && j < pattern[i][4])
				{
					j = pattern[i][4]; //j=2枚以下の組でxが最大のものを記録
				}
			}
			if (j <= 90 && pattern[11][4] >= pattern[11][5])
			{
				k = 1; //強いカードなし、革命時のほうが強い
			}
			else if (j <= 90 && pattern[11][4] < pattern[11][5])
			{
				k = 2; //強いカードなし、革命時のほうが弱い
			}
			else if (j >= 91 && pattern[11][4] >= pattern[11][5])
			{
				k = 3; //強いカードあり、革命時のほうが強い
			}
			else
			{
				k = 4; //強いカードあり、革命時のほうが弱い
			}
		}
		if (k == 1)
		{
			//革命を起こす
			for (i = 0; i < pattern[11][0]; i++)
			{
				if (pattern[i][1] >= 5 || (pattern[i][1] == 4 && (pattern[i][3] == 0 || pattern[i][3] == 14)))
				{
					pattern[i][6] = 90;
				}
				else if (pattern[i][4] >= STRONG && pattern[i][4] <= 100 && pattern[i][3] != 14)
				{
					pattern[i][6] = 91;
				}
			}
		}
		else if (k == 4)
		{
			//革命を起こさない
			for (i = 0; i < pattern[11][0]; i++)
			{
				if (pattern[i][1] >= 5 || (pattern[i][1] == 4 && (pattern[i][3] == 0 || pattern[i][3] == 14)))
				{
					pattern[i][6] = 8;
				}
			}
		}
		else if (k == 3)
		{
			//弱いカードを出さないよう考える(この時点では起こさない)
			for (i = 0; i < pattern[11][0]; i++)
			{
				if (pattern[i][0] <= 5)
				{
					pattern[i][6] = 7;
				}
				if (pattern[i][1] >= 5 || (pattern[i][1] == 4 && (pattern[i][3] == 0 || pattern[i][3] == 14)))
				{
					pattern[i][6] = 8;
				}
			}
		}
		else
		{	//k==2
			//特に変更なし
		}

		//表示
		if (PRINT_PAT == 2)
		{
			for (i = 0; i < 11; i++)
			{
				if (pattern[i][1] == 0)
				{
					i = 11;
				}
				else
				{
					fprintf(stderr, "[ord%2d][qty%d][sui%2d][Jok%2d][x%3d][rx%3d][y%2d]\n", pattern[i][0], pattern[i][1], pattern[i][2], pattern[i][3], pattern[i][4], pattern[i][5], pattern[i][6]);
				}
			}
			fprintf(stderr, "[set%2d][rev%d][1min%3d][2min%3d][z%3d][rz%3d][r2m%3d]\n", pattern[11][0], pattern[11][1], pattern[11][2], pattern[11][3], pattern[11][4], pattern[11][5], pattern[11][6]);
			fprintf(stderr, "\n");
		}

		pattern[12][5] = -1;
		for (i = 0; i < pattern[11][0]; i++)
		{
			//手札の組からyが一番大きいものを選ぶ
			if (pattern[12][6] < pattern[i][6])
			{
				pattern[12][0] = pattern[i][0];
				pattern[12][1] = pattern[i][1];
				pattern[12][2] = pattern[i][2];
				pattern[12][3] = pattern[i][3];
				pattern[12][4] = pattern[i][4];
				pattern[12][5] = pattern[i][5];
				pattern[12][6] = pattern[i][6];
			}
		}
		if (pattern[11][3] == 201 && pattern[12][3] == 0)
		{ //残り1組
			j = 0;
			for (i = 0; i < pattern[11][0]; i++)
			{
				if (pattern[i][3] == 14 && pattern[i][2] == 0)
				{
					j = 1;
				}
			}
			if (j == 1)
			{ //ジョーカーを含めて出す
				pattern[12][1] += 1;
				pattern[12][3] = 14;
				if (pattern[12][2] % 2 != 1)
				{
					pattern[12][2] += 1;
				}
				else if (pattern[12][2] % 4 <= 1)
				{
					pattern[12][2] += 2;
				}
				else if (pattern[12][2] % 8 <= 3)
				{
					pattern[12][2] += 4;
				}
				else if (pattern[12][2] < 8)
				{
					pattern[12][2] += 8;
				}
			}
		}
		else if (pattern[12][4] >= STRONG && pattern[12][4] <= 100 && pattern[12][3] == 0 && pattern[12][1] == 1)
		{ //あがり手選択の場合(単体)
			j = 0;
			pattern[12][1] = 0;
			pattern[12][2] = 0;
			for (i = 0; i < pattern[11][0]; i++)
			{
				if (pattern[i][0] == pattern[12][0] && pattern[i][3] == 0)
				{
					pattern[12][1] += 1;
					pattern[12][2] += pattern[i][2];
				}
				if (pattern[i][3] == 14 && pattern[i][2] == 0)
				{ //ジョーカーを階段に使用していない場合
					j = 1;
				}
			}
			if (pattern[12][1] == 4)
			{ //4枚あるときはスペードとハートの2枚のみ出す
				pattern[12][1] = 2;
				pattern[12][2] = 3;
			}
			if (pattern[12][1] != 3 && j == 1)
			{
				//ジョーカーを含めて出す
				pattern[12][1] += 1;
				pattern[12][3] = 14;
				if (pattern[12][2] % 2 != 1)
				{
					pattern[12][2] += 1;
				}
				else if (pattern[12][2] % 4 <= 1)
				{
					pattern[12][2] += 2;
				}
				else if (pattern[12][2] % 8 <= 3)
				{
					pattern[12][2] += 4;
				}
				else if (pattern[12][2] < 8)
				{
					pattern[12][2] += 8;
				}
			}
		}
		//patternからselect_cardsに提出手を入れる
		if (pattern[12][3] == 14 && pattern[12][2] == 0)
		{ //ジョーカーの場合
			select_cards[0][0] = 2;
		}
		else if (pattern[12][3] == 14 && pattern[12][2] != 0)
		{ //ジョーカー込の枚数組
			if (pattern[12][2] % 2 == 1)
			{
				select_cards[0][pattern[12][0]] = 1;
				if (own_cards[0][pattern[12][0]] == 0)
				{
					select_cards[0][pattern[12][0]] = 2;
				}
			}
			if (pattern[12][2] % 4 >= 2)
			{
				select_cards[1][pattern[12][0]] = 1;
				if (own_cards[1][pattern[12][0]] == 0)
				{
					select_cards[1][pattern[12][0]] = 2;
				}
			}
			if (pattern[12][2] % 8 >= 4)
			{
				select_cards[2][pattern[12][0]] = 1;
				if (own_cards[2][pattern[12][0]] == 0)
				{
					select_cards[2][pattern[12][0]] = 2;
				}
			}
			if (pattern[12][2] >= 8)
			{
				select_cards[3][pattern[12][0]] = 1;
				if (own_cards[3][pattern[12][0]] == 0)
				{
					select_cards[3][pattern[12][0]] = 2;
				}
			}
		}
		else if (pattern[12][3] != 0)
		{ //階段組
			if (pattern[12][2] == 1)
			{
				k = 0;
			}
			else if (pattern[12][2] == 2)
			{
				k = 1;
			}
			else if (pattern[12][2] == 4)
			{
				k = 2;
			}
			else
			{
				k = 3;
			}
			for (j = 0; j < pattern[12][1]; j++)
			{
				if (own_cards[k][pattern[12][0] - j] == 1)
				{
					select_cards[k][pattern[12][0] - j] = 1;
				}
				else
				{
					select_cards[k][pattern[12][0] - j] = 2;
				}
			}
		}
		else
		{ //ジョーカーなしの枚数組
			if (pattern[12][2] % 2 == 1)
			{
				select_cards[0][pattern[12][0]] = 1;
			}
			if (pattern[12][2] % 4 >= 2)
			{
				select_cards[1][pattern[12][0]] = 1;
			}
			if (pattern[12][2] % 8 >= 4)
			{
				select_cards[2][pattern[12][0]] = 1;
			}
			if (pattern[12][2] >= 8)
			{
				select_cards[3][pattern[12][0]] = 1;
			}
		}
		for (i = 0; i < 4; i++)
		{
			for (j = 0; j <= 14; j++)
			{
				if (select_cards[i][j] == 1 && own_cards[i][j] == 0)
				{
					select_cards[i][j] = 2;
				}
			}
		}
	}
}

void kou_followgroup(int select_cards[][15], int own_cards[8][15], int max, int used_cards[8][15], int tes[3][4][3])
{
	//続いてカードを枚数組で出すときの思考
	int i, j, k;
	int p = 0;				  //プレイヤ数
	int ord = state.ord;	  //現在見ているカードの強さ
	int card_flag = 1;		  //探索終了まで1
	int target_cards[8][15];  //出すカードの優先候補を入れる
	int use_own_cards[8][15]; //出した後の状態を入れる
	int search[8][15];
	int suit[5] = {0};				//スート(suit[4]はスートの組み合わせパターンの順番)
	int pattern[13][7] = {{0}};		//カードを選ぶ前の手札のパターン
	int use_pattern[13][7] = {{0}}; //カードを選び出した後の手札のパターン
	int own_num[15] = {0};			//自分のカードの枚数を記録
	for (i = 1; i <= 13; i++)
	{
		own_num[i] = own_cards[0][i] + own_cards[1][i] + own_cards[2][i] + own_cards[3][i];
	}
	pat_make(pattern, own_cards, max, used_cards, state.joker, tes); //現在の手札の組を作る
	if (state.lock == 1)
	{ //縛りのとき
		for (i = 0; i < 4; i++)
		{
			suit[i] = state.suit[i];
		}
	}
	for (i = 0; i < 5; i++)
	{ //あがっていない人数カウント
		if (own_cards[6][i] > 0)
		{
			p++;
		}
	}
	if (state.rev == 0)
	{
		ord++;
	}
	else
	{
		ord--;
	}
	//場のカードの次に強いカードの強さから探す
	clearTable(select_cards);
	clearTable(target_cards);
	suit[4] = 0;
	if (ord > 13 || ord < 1)
	{
		card_flag = 0;
	}
	while (card_flag == 1) //フラグが1の間
	{
		//選ぶスートのパターンをすべて調べる
		clearTable(target_cards);
		copyCards(search, own_cards); //searchにownを入れる
		if (state.lock == 1)
		{ //縛りの場合は1パターンのみ
			for (i = 0; i < 4; i++)
			{
				suit[i] = state.suit[i];
			}
		}
		else
		{ //すべての出せるパターンを調べる
			if (state.qty == 1)
			{
				suit[0] = 0;
				suit[1] = 0;
				suit[2] = 0;
				suit[3] = 0;
				suit[suit[4]] = 1;
			}
			if (state.qty == 3)
			{
				suit[0] = 1;
				suit[1] = 1;
				suit[2] = 1;
				suit[3] = 1;
				suit[suit[4]] = 0;
			}
			if (state.qty == 2)
			{
				if (suit[4] <= 2)
				{
					suit[0] = 1;
				}
				else
				{
					suit[0] = 0;
				}
				if (suit[4] == 0 || suit[4] == 3 || suit[4] == 4)
				{
					suit[1] = 1;
				}
				else
				{
					suit[1] = 0;
				}
				if (suit[4] % 2 == 1)
				{
					suit[2] = 1;
				}
				else
				{
					suit[2] = 0;
				}
				if (suit[4] == 2 || suit[4] == 4 || suit[4] == 5)
				{
					suit[3] = 1;
				}
				else
				{
					suit[3] = 0;
				}
			}
			if (state.qty == 4)
			{
				suit[0] = 1;
				suit[1] = 1;
				suit[2] = 1;
				suit[3] = 1;
			}
		} //スートのパターンの探索ここまで
		if (pattern[12][2] >= 1)
		{
			pattern[12][2] = 1;
		}
		//出せるかどうかを判定する
		for (i = 0; i < 4; i++)
		{
			if (suit[i] == 1 && own_cards[i][ord] == 0)
			{
				if (pattern[12][2] == 0)
				{ //ジョーカーなし
					i = 6;
				}
				else if (pattern[12][2] == 1)
				{ //ジョーカーがある場合
					if (state.qty == 1)
					{		   //場の枚数が1枚
						i = 6; //ジョーカー単体は別の判定をするので、ここでは選ばない
					}
					else
					{
						pattern[12][2] = 3; //使おうと考える
					}
				}
				else if (pattern[12][2] == 3)
				{ //持っていないスートが2つ以上
					pattern[12][2] = 1;
					i = 6;
				}
			}
		}
		if (i <= 5) //出せる場合
		{
			for (i = 0; i < 4; i++)
			{
				if (suit[i] == 1 && own_cards[i][ord] == 1)
				{
					target_cards[i][ord] = 1;
				}
				else if (suit[i] == 1 && own_cards[i][ord] == 0)
				{
					target_cards[i][ord] = 2;
				}
			}
			clearTable(use_own_cards);
			copyCards(use_own_cards, own_cards);	//use_ownにownを入れる
			cardsDiff(use_own_cards, target_cards); //use_ownからtarget(選択した手)を除く
			clearTable(search);
			copyCards(search, used_cards); //searchにused_cards(すでに場に出たカード)を入れる
			for (i = 0; i <= 4; i++)
			{ //searchに出すカードを入れる(選択した手を提出した場合の場に出たカード)
				for (j = 0; j <= 14; j++)
				{
					if (target_cards[i][j] == 1)
					{
						search[i][j] = 1;
					}
					if (target_cards[i][j] == 2)
					{
						search[4][14] = 1;
					}
				}
			}
			for (j = 1; j <= 13; j++)
			{
				search[4][j] = search[0][j] + search[1][j] + search[2][j] + search[3][j];
			}
			for (i = 0; i < 5; i++)
			{
				use_own_cards[6][i] = own_cards[6][i]; //手札枚数の情報を入れる
			}
			use_pattern[12][2] = pattern[12][2];									//ジョーカーを使用したかどうかを入れる
			pat_make(use_pattern, use_own_cards, max, search, pattern[12][2], tes); //提出した場合の手札の組を作る
			//縛りの判定
			if (ord == 6)
			{ //8切りの場合
				i = 6;
			}
			else if (state.rev == 0 && ord >= max)
			{ //場で一番強いカードの場合
				i = 6;
			}
			else if (state.rev == 1 && ord <= max)
			{
				i = 6;
			}
			else if (state.lock == 1)
			{		   //縛り状態の場合
				i = 5; //縛りの判定をする
			}
			else
			{ //縛りでない場合
				for (i = 0; i < 4; i++)
				{ //出す場合,縛りになるかどうか調べる
					if (state.suit[i] != suit[i])
					{ //縛りにならない場合
						i = 6;
					}
				}
			}
			/*
      i=5…縛りの場合の判定をする
      i=6…通常(縛りでない場合)の判定をする
      */
			if (i <= 5 && state.qty != 1 && p >= 3)
			{ //縛り状態か縛りの場合で場が2枚以上、相手が2人以上なら積極的に出す
				i = 5;
			}
			else if (i <= 5)
			{ //縛り状態か縛りにする場合
				if (state.rev == 0)
				{ //通常時
					for (j = max; j > ord; j--)
					{
						for (i = 0; i < 4; i++)
						{
							if (suit[i] > 0 && used_cards[i][j] + own_cards[i][j] != 0)
							{ //相手が出せる可能性があるかどうか調べる
								i = 6;
							}
						}
						if (i <= 5)
						{		   //相手が出せる可能性がある場合
								   //if(state.lock==1)
							i = 6; //縛り状態のときは出す優先度は変えない
							//else
							//  i=7;//積極的には出さない
							j = 0; //相手が出せる可能性があるときはすぐループを抜ける
						}
						else
						{ //相手が出せる可能性がない場合
							for (i = 0; i < 4; i++)
							{
								if (suit[i] > 0 && own_cards[i][j] == 0)
								{ //自分が同じスートで強いカードを持っていない場合
									i = 6;
								}
							}
							if (i <= 5)
							{ //自分が同じスートで強いカードを持っている場合
								if (use_pattern[12][2] == 3)
								{
									i = 6; //ジョーカー使用の場合は出す優先度は変えない
								}
								else
								{
									i = 5; //積極的に出す
								}
								j = 0; //ループを抜ける
							}
							else if (ord == j - 1)
							{		   //同じスートで強いカードがない場合(出すカードがそのスートで一番強い場合)
								i = 5; //積極的に出す
								j = 0; //ループを抜ける
							}
						}
					}
				}
				else
				{ //革命時も同じように判断
					for (j = max; j < ord; j++)
					{
						for (i = 0; i < 4; i++)
						{
							if (suit[i] > 0 && used_cards[i][j] + own_cards[i][j] != 0)
							{
								i = 6;
							}
						}
						if (i <= 5)
						{
							//if(state.lock==1)
							i = 6;
							//else
							//  i=7;
							j = 14;
						}
						else
						{
							for (i = 0; i < 4; i++)
							{
								if (suit[i] > 0 && own_cards[i][j] == 0)
								{
									i = 6;
								}
							}
							if (i <= 5)
							{
								if (use_pattern[12][2] == 3)
								{
									i = 6;
								}
								else
								{
									i = 5;
								}
								j = 14;
							}
							else if (ord == j + 1)
							{
								i = 5;
								j = 14;
							}
						}
					}
				}
			}
			else
			{ //縛りでない場合
				i = 6;
			} //縛りの判定ここまで
			/*
      i=5…複数枚で縛る場合か,1枚で縛る場合で一番強いカードを持っている場合
      i=7…1枚で縛る場合で一番強いカードを持っていない場合(削除)
      i=6…上記以外
      */
			//選んだカードの評価値(x')の決定
			if (i == 6)
			{
				use_pattern[12][0] = 100; //基準
				if (ord == 6)
				{ //8切り
					use_pattern[12][0] = 101;
				}
				else if (state.qty == 1)
				{ //単体の場合、相手が出せる組があるとき-30する
					if (state.rev == 0)
					{
						for (j = max; j > ord; j--)
						{
							if ((4 - used_cards[4][j] - own_num[j]) >= 1)
							{
								use_pattern[12][0] -= 30;
							}
						}
					}
					else
					{
						for (j = max; j < ord; j++)
						{
							if ((4 - used_cards[4][j] - own_num[j]) >= 1)
							{
								use_pattern[12][0] -= 30;
							}
						}
					}
					if (used_cards[4][14] == 0 && state.joker == 0)
					{
						use_pattern[12][0] -= 1;
					}
				} //単体の場合の判定ここまで
				else
				{ //複数枚の場合
					if (state.rev == 0)
					{
						for (j = max; j > ord; j--)
						{
							if ((4 - used_cards[4][j] - own_num[j]) == (state.qty - 1) && used_cards[4][14] == 0 && state.joker == 0)
							{							 //ジョーカーを含めると出される場合
								use_pattern[12][0] -= 1; //1だけ減らす
							}
							else if ((4 - used_cards[4][j] - own_num[j]) <= (state.qty - 1))
							{	//出されるパターンなし
								//減らさない
							}
							else
							{
								k = (4 - used_cards[4][j] - own_num[j]) - state.qty - p + 5;
								if (p == 2)
								{
									use_pattern[12][0] -= 30;
								}
								else if (k <= 0)
								{
									use_pattern[12][0] -= 4;
								}
								else if (k == 1)
								{
									use_pattern[12][0] -= 9;
								}
								else if (k == 2)
								{
									use_pattern[12][0] -= 15;
								}
								else
								{
									use_pattern[12][0] -= 24;
								}
							}
						}
					}
					else
					{
						for (j = max; j < ord; j++)
						{
							if ((4 - used_cards[4][j] - own_num[j]) == (state.qty - 1) && used_cards[4][14] == 0 && state.joker == 0)
							{							 //ジョーカーを含めると出される場合
								use_pattern[12][0] -= 1; //1だけ減らす
							}
							else if ((4 - used_cards[4][j] - own_num[j]) <= (state.qty - 1))
							{	//出されるパターンなし
								//減らさない
							}
							else
							{
								k = (4 - used_cards[4][j] - own_num[j]) - state.qty - p + 5;
								if (p == 2)
								{
									use_pattern[12][0] -= 30;
								}
								else if (k <= 0)
								{
									use_pattern[12][0] -= 4;
								}
								else if (k == 1)
								{
									use_pattern[12][0] -= 9;
								}
								else if (k == 2)
								{
									use_pattern[12][0] -= 15;
								}
								else
								{
									use_pattern[12][0] -= 24;
								}
							}
						}
					}
				} //複数枚の場合の判定ここまで
				if (use_pattern[12][0] <= 0)
				{
					use_pattern[12][0] = 1;
				}
			}
			else if (i == 5)
			{ //複数枚で縛る場合か,1枚で縛る場合で一番強いカードを持っている場合はx'=STRONG
				use_pattern[12][0] = STRONG;
			}
			else
			{							//もし予想外の動作が起きた場合
				use_pattern[12][0] = 1; //x'=1にする
			}
			//出した後の全体評価値(z')の決定
			use_pattern[12][1] = use_pattern[11][4];
			//x'によってz'を変化
			if (use_pattern[12][0] >= STRONG)
			{ //場を流せる可能性が高い場合1減らす
				use_pattern[12][1] -= 1;
			}
			if (i != 5 && use_pattern[12][0] < STRONG && use_pattern[11][0] >= pattern[11][0])
			{ //場を流せる可能性が低く、組数が減らない場合1増やす
				use_pattern[12][1] += 1;
			}
			//出すかどうか判定
			j = 1;
			if (use_pattern[12][0] >= STRONG && use_pattern[11][3] >= STRONG)
			{
				//x>=STRONG,2min>=STRONG →出す(あがりの形の場合は出す)
				j = 1;
			}
			else if (use_pattern[12][0] == 101 && use_pattern[11][0] <= pattern[11][0])
			{
				//8を含む組で組数が減るか同じ →出す(8の場合、組数が増えない場合は出す)
				j = 1;
			}
			else if (use_pattern[12][0] > STRONG && use_pattern[11][0] >= 4 && use_pattern[11][0] - 1 <= use_pattern[12][1])
			{
				//x>=STRONG,組数4以上,組数-1<=z'→出さない(場を流せる場合で、他に強い組がない場合出さない)
				j = 0;
			}
			else if (use_pattern[11][2] <= 50 && use_pattern[11][3] == 201 && use_pattern[12][0] <= 60)
			{
				j = 0; //1min<=50,2min=201,x'<=60 →出さない(残り1組の場合で場を流せる可能性が低い場合は出さない)
			}
			else if (use_pattern[12][2] == 3 && use_pattern[12][0] <= STRONG)
			{
				//ジョーカー使用の場合でx<=STRONG(2min>=STRONG,x=STRONGは出す) →出さない
				j = 0;
			}
			else if (use_pattern[11][3] >= STRONG)
			{
				//2min>=STRONG →出す(↑の場合以外で、あがりの形になる場合は出す)
				j = 1;
			}
			//else if(state.qty==1 && use_pattern[12][0]==31 && use_pattern[12][1]<=pattern[11][4]){
			//  j=2;//縛りで強いカードを持っていない場合は出すが、優先度は低くする
			//}
			else if (state.qty == 1 && use_pattern[12][1] >= 4 && use_pattern[12][0] >= 40 && use_pattern[12][0] <= 70)
			{
				j = 0; //qty=1,z'>=4,40<=x'<=70 →出さない(手札が多いうちは単体のKやAは出さない)
			}
			else if (use_pattern[12][1] > pattern[11][4])
			{
				//z'>z →出さない(↑以外で、zが大きくなる場合は出さない)
				j = 0;
			}
			else
			{
				j = 1; //組数が減り,zが大きくならない場合は大体出す
			}
			if (PRINT_PAT == 2)
			{
				fprintf(stderr, "ord%2d,suit%4d ", ord, (suit[0] * 1000) + (suit[1] * 100) + (suit[2] * 10) + suit[3]);
				fprintf(stderr, "1min%3d,2min%3d ", use_pattern[11][2], use_pattern[11][3], use_pattern[11][4]);
				fprintf(stderr, "x'%3d\n", use_pattern[12][0]);
				fprintf(stderr, "pair%2d->%2d,z'%3d->%3d [check%2d]\n", pattern[11][0], use_pattern[11][0], pattern[11][4], use_pattern[12][1], j);
			}
			if (beEmptyCards(select_cards) == 1 && j >= 1)
			{										   //これまでの判定で出す候補がない場合
				copyCards(select_cards, target_cards); //出す判定
				pattern[12][0] = use_pattern[12][0];
				pattern[12][1] = use_pattern[12][1]; //+j-1
			}
			else if (j >= 1)
			{ //出す候補がある場合
				if (use_pattern[11][3] >= STRONG && pattern[12][0] >= STRONG && use_pattern[12][0] < STRONG)
				{
					//2min>=STRONG,変更前の手のx>=STRONG 変更後の手のx<STRONG→出さない
				}
				else if (use_pattern[11][3] >= STRONG && use_pattern[12][0] >= STRONG && pattern[12][0] < STRONG)
				{
					//2min>=STRONG,変更後の手のx>=STRONG,変更前の手のx<STRONG →出す
					clearCards(select_cards);
					copyCards(select_cards, target_cards); //出す判定
					pattern[12][0] = use_pattern[12][0];
					pattern[12][1] = use_pattern[12][1];
				}
				else if (use_pattern[11][3] >= STRONG && use_pattern[12][0] > STRONG && pattern[12][0] == STRONG)
				{
					//2min>=STRONG,変更後の手のx>STRONG,変更前の手のx=STRONG →出す(あがり判定のときは「縛り」ではなく、強いカードで場を流す)
					clearCards(select_cards);
					copyCards(select_cards, target_cards); //出す判定
					pattern[12][0] = use_pattern[12][0];
					pattern[12][1] = use_pattern[12][1];
				}
				else if ((use_pattern[12][1] + j - 1) < pattern[12][1])
				{
					//変更後の手のz<変更前の手のz →出す
					clearCards(select_cards);
					copyCards(select_cards, target_cards); //出す判定
					pattern[12][0] = use_pattern[12][0];
					pattern[12][1] = use_pattern[12][1];
				}
			}
		} //選んだカードが出せる場合の判定ここまで
		  //次のスートの組み合わせを出す
		clearTable(target_cards);
		suit[4]++;
		if (state.lock == 1)
		{
			suit[4] = 0;
		}
		else if (state.qty == 1 || state.qty == 3)
		{
			if (suit[4] == 4)
			{
				suit[4] = 0;
			}
		}
		else if (state.qty == 2 && suit[4] == 6)
		{
			suit[4] = 0;
		}
		else if (state.qty == 4)
		{
			suit[4] = 0;
		}
		if (suit[4] == 0 && state.rev == 0)
		{ //スートの組み合わせを調べ終わったら次の数字にする
			ord++;
		}
		else if (suit[4] == 0)
		{
			ord--;
		}
		if (ord > 13 || ord < 1)
		{ //出せる組み合わせを調べ終わったらループを抜ける
			card_flag = 0;
		}
	} //ジョーカー単体以外の判定終了

	//場のカードが1枚のときジョーカー単体について考える
	if (state.qty == 1 && state.joker == 1)
	{
		//use_pattern[12][2]=3;//ジョーカー使用
		pat_make(use_pattern, own_cards, max, used_cards, 0, tes); //ジョーカー使用時のパターンを作る
		if ((pattern[11][0] >= use_pattern[11][0] && state.ord == max) || use_pattern[11][3] >= STRONG)
		{ //場の数字が最大で組数が増えない または 2min>=STRONG
			if (used_cards[0][1] + own_cards[0][1] == 1)
			{ //スペード3が出ていない
				if (beEmptyCards(select_cards) == 1 && j >= 1)
				{ //これまでの判定で出す候補がない場合
					clearTable(select_cards);
					select_cards[0][0] = 2; //出す
				}
				else if (pattern[12][0] < STRONG)
				{
					//選択していた手のx<STRONG →出す
					clearTable(select_cards);
					select_cards[0][0] = 2; //出す
				}
			}
		}
	}
}

void kou_followsequence(int select_cards[][15], int own_cards[8][15], int max, int used_cards[8][15], int tes[3][4][3])
{
	//階段で出すときの思考
	int i, j;
	int suit, ord; //現在見ているスート,強さ
	int count;
	int jk = -1;			  //ジョーカー位置
	int card_flag = 1;		  //出せるカードがある間1
	int target_cards[8][15];  //出すカードの優先候補を入れる
	int use_own_cards[8][15]; //出した後の状態を入れる
	int search[8][15];
	int pattern[13][7] = {{0}};										 //カードを選ぶ前の手札のパターン
	int use_pattern[13][7] = {{0}};									 //カードを選び出した後の手札のパターン
	pat_make(pattern, own_cards, max, used_cards, state.joker, tes); //現在の手札のパターンを作る
	int seq_max;
	int seq_min;
	if (state.rev == 0)
	{
		seq_max = 14;
		seq_min = state.ord + state.qty;
	}
	else
	{
		seq_max = state.ord - 1;
		seq_min = state.qty - 1;
	}
	clearTable(select_cards);
	clearTable(target_cards);
	clearTable(search);

	for (suit = 0; suit < 4; suit++)
	{
		if (state.lock == 1)
		{ //縛りのとき
			while (state.suit[suit] != 1 && suit < 4)
			{
				suit++;
			}
		}
		for (ord = seq_max; ord >= seq_min && suit < 4; ord--)
		{
			jk = -1;
			count = 0;
			clearTable(target_cards);
			while (count != state.qty && (ord - count) >= 0)
			{
				if (own_cards[suit][ord - count] == 1)
				{
					target_cards[suit][ord - count] = 1;
					count++;
				}
				else if (jk == -1 && own_cards[4][1] == 2)
				{
					target_cards[suit][ord - count] = 2;
					jk = ord - count;
					count++;
				}
				else
				{
					break;
				}
			}
			if (count == state.qty)
			{ //階段発見の場合

				clearTable(use_own_cards);
				copyCards(use_own_cards, own_cards);	//use_ownにownを入れる
				cardsDiff(use_own_cards, target_cards); //use_ownからtarget(選択した手)を除く
				clearTable(search);
				copyCards(search, used_cards); //searchにused_cards(すでに場に出たカード)を入れる
				for (i = 0; i <= 4; i++)
				{ //searchに出すカードを入れる(選択した手を提出した場合の場に出たカード)
					for (j = 0; j <= 14; j++)
					{
						if (target_cards[i][j] == 1)
						{
							search[i][j] = 1;
						}
						if (target_cards[i][j] == 2)
						{
							search[4][14] = 1;
						}
					}
				}
				for (j = 1; j <= 13; j++)
				{
					search[4][j] = search[0][j] + search[1][j] + search[2][j] + search[3][j];
				}
				for (i = 0; i < 5; i++)
				{
					use_own_cards[6][i] = own_cards[6][i]; //手札枚数の情報を入れる
				}
				if (jk != -1)
				{ //ジョーカーを使用した場合
					use_pattern[12][2] = 3;
				}
				pat_make(use_pattern, use_own_cards, max, search, use_pattern[12][2], tes); //提出した場合の手札のパターンを作る
				//出すかどうか判定
				j = 0;
				if (use_pattern[11][3] >= STRONG)
				{
					//2min>=STRONG →出す(あがりの形になる場合は出す)
					j = 1;
				}
				else if (use_pattern[11][4] <= pattern[11][4] + 1)
				{
					j = 1;
				}
				if (beEmptyCards(select_cards) == 1 && j >= 1)
				{										   //これまでの判定で出す候補がない場合
					copyCards(select_cards, target_cards); //出す判定
					//pattern[12][0]=99;
					pattern[12][1] = use_pattern[11][4];
				}
				else if (j >= 1)
				{ //出す候補がある場合
					if (use_pattern[12][1] < pattern[12][1])
					{
						//変更後の手のz<変更前の手のz →出す
						clearCards(select_cards);
						copyCards(select_cards, target_cards); //出す判定
						//pattern[12][0]=99;
						pattern[12][1] = use_pattern[11][4];
					}
				}

			} //階段発見の場合ここまで
		}
	}
}

void kou_change(int out_cards[8][15], int my_cards[8][15], int num_of_change, int tes[3][4][3])
{
	//カード交換の判定
	int i, j, k;
	int search[8][15];
	int cards[15][3] = {{0}};	   //[0]強さ[1]スート(0,1,2,3)[2]手札から選んだカードを除いた場合のz
	int used_cards[8][15] = {{0}}; //提出後のカード 何も入ってないので0
	int pattern[13][7] = {{0}};
	clearTable(out_cards);
	clearTable(search);
	int c;
	k = 0;
	for (j = 1; j <= 12; j++)
	{ //3からAまで
		for (i = 0; i < 4; i++)
		{
			if (my_cards[i][j] == 1)
			{
				cards[k][0] = j;
				cards[k][1] = i;
				k++;
			}
		}
	}
	for (i = 0; i < 4; i++)
	{
		my_cards[i][13] = 0;
	}
	for (i = 0; i < k; i++)
	{
		clearTable(search);
		copyTable(search, my_cards);
		search[cards[i][1]][cards[i][0]] = 0;
		pat_make(pattern, search, 13, used_cards, state.joker, tes);
		cards[i][2] = (pattern[11][4] * 10) + cards[i][0];
		if (cards[i][0] == 1)
		{
			cards[i][2] += 5;
			if (cards[i][1] == 0 || cards[i][2] == 2)
			{
				cards[i][2] += 20;
			}
		}
		if (cards[i][0] == 6)
		{
			cards[i][2] += 30;
		}
		if (cards[i][0] == 11)
		{
			cards[i][2] += 15;
		}
		if (cards[i][0] == 12)
		{
			cards[i][2] += 30;
		}
	}
	for (i = 0; i < k; i++)
	{
		for (j = i; j > 0; j--)
		{
			if (cards[j][2] < cards[j - 1][2])
			{
				cards[14][0] = cards[j][0];
				cards[14][1] = cards[j][1];
				cards[14][2] = cards[j][2];
				cards[j][0] = cards[j - 1][0];
				cards[j][1] = cards[j - 1][1];
				cards[j][2] = cards[j - 1][2];
				cards[j - 1][0] = cards[14][0];
				cards[j - 1][1] = cards[14][1];
				cards[j - 1][2] = cards[14][2];
			}
		}
	}
	out_cards[cards[0][1]][cards[0][0]] = 1;
	if (num_of_change == 2)
	{
		out_cards[cards[1][1]][cards[1][0]] = 1;
	}
}

void pat_make(int pattern[][7], int own_cards[8][15], int max, int used_cards[8][15], int joker_flag, int tes[3][4][3])
{
	//手札の組作りと評価値の決定
	int i, j, k;
	int select_cards[8][15];
	int search[8][15];
	int t; //0:前半1:中2:後半
	clearTable(search);
	for (i = 0; i < 13; i++)
	{
		for (j = 0; j < 7; j++)
		{
			pattern[i][j] = 0;
		}
	}
	int own_cards_copy[8][15]; //探索用手札テーブル
	copyTable(own_cards_copy, own_cards);
	if (joker_flag == 2)
	{ //フラグが2(念の為)の場合1
		joker_flag = 1;
	}
	if (joker_flag == 3)
	{ //フラグが3(提出手で使用)の場合0
		joker_flag = 0;
	}
	//カードの組を作る
	i = 0;
	j = 1;
	while (j != 0)
	{
		k = setmake(search, own_cards_copy, joker_flag);
		j = kaidanhand(select_cards, search, own_cards_copy); //階段を作る
		if (j != 0)
		{ //階段
			cardsDiff(own_cards_copy, select_cards);
			pattern[i][0] = j;
			pattern[i][3] = 0;
			for (pattern[i][2] = 0; select_cards[pattern[i][2]][j] == 0; pattern[i][2]++)
			{
				//スートを調べる
			}
			for (; j >= 0; j--)
			{
				if (select_cards[pattern[i][2]][j] == 2)
				{
					pattern[i][3] = j;
					pattern[12][2] = 2;
					joker_flag = 0;
				}
				else if (select_cards[pattern[i][2]][j] == 0)
				{
					pattern[i][1] = pattern[i][0] - j;
					j = 0;
				}
			}
			if (pattern[i][2] == 0)
			{
				pattern[i][2] = 1;
			}
			else if (pattern[i][2] == 1)
			{
				pattern[i][2] = 2;
			}
			else if (pattern[i][2] == 2)
			{
				pattern[i][2] = 4;
			}
			else if (pattern[i][2] == 3)
			{
				pattern[i][2] = 8;
			}
			if (pattern[i][3] == 0)
			{
				pattern[i][3] = 15;
			}
			i++;
			j = -1;
		}
		else
		{ //枚数組
			clearTable(select_cards);
			j = grouphand(select_cards, search, 0); //弱い枚数組を作る

			if (j >= 1)
			{ //8,強カード以外の枚数組
				cardsDiff(own_cards_copy, select_cards);
				pattern[i][0] = j;
				pattern[i][1] = select_cards[0][j] + select_cards[1][j] + select_cards[2][j] + select_cards[3][j];
				pattern[i][3] = 0;
				if (pattern[i][1] == 4 && state.rev == 0 && j >= max)
				{ //強カード4枚の場合,2枚ずつの組にする
					pattern[i][1] = 2;
					pattern[i][2] = 3;
					i++;
					pattern[i][0] = j;
					pattern[i][1] = 2;
					pattern[i][2] = 12;
					pattern[i][3] = 0;
				}
				else if (pattern[i][1] == 4 && state.rev == 1 && j <= max)
				{ //強カード4枚の場合(革命時)
					pattern[i][1] = 2;
					pattern[i][2] = 3;
					i++;
					pattern[i][0] = j;
					pattern[i][1] = 2;
					pattern[i][2] = 12;
					pattern[i][3] = 0;
				}
				else
				{
					pattern[i][2] = 0;
					if (select_cards[0][j] == 1)
					{
						pattern[i][2] += 1;
					}
					if (select_cards[1][j] == 1)
					{
						pattern[i][2] += 2;
					}
					if (select_cards[2][j] == 1)
					{
						pattern[i][2] += 4;
					}
					if (select_cards[3][j] == 1)
					{
						pattern[i][2] += 8;
					}
				}
				i++;
				j = -1;
			}
			if (j == 0 && joker_flag == 1)
			{ //ジョーカー単体
				pattern[i][1] = 1;
				pattern[i][3] = 14;
				pattern[12][2] = 1;
				joker_flag = 0;
				i++;
			}
		}
		clearTable(search);
		clearTable(select_cards);
	}
	//組数格納
	for (i = 0; i < 11; i++)
	{
		if (pattern[i][1] == 0)
		{
			pattern[11][0] = i;
			i = 12;
		}
	}
	if (i == 11)
	{
		pattern[11][0] = 11;
	}
	//x,revx,z,revz,1min,2minの決定
	value_strong(pattern, own_cards, used_cards, state.rev, 4, tes);
	value_strong(pattern, own_cards, used_cards, (state.rev + 1) % 2, 5, tes);
	//評価値yの決定
	for (i = 0; i < pattern[11][0]; i++)
	{
		if (pattern[i][3] == 14)
		{
			pattern[i][6] = 3;
		}
		else if (pattern[i][4] >= STRONG && pattern[i][4] <= 100)
		{										 //流れる可能性が高い場合
			pattern[i][6] = 110 - pattern[i][4]; //10～15
		}
		else if (pattern[i][4] > 100 && pattern[i][3] != 0)
		{ //階段
			pattern[i][6] = 9;
		}
		else if (pattern[i][4] >= 1 && pattern[i][4] <= 200)
		{
			if (state.rev == 0)
			{
				pattern[i][6] = 40 + (2 * (14 - pattern[i][0])); //44～64
			}
			else
			{
				pattern[i][6] = 40 + (2 * pattern[i][0]); //44～64
			}
			if (pattern[i][4] > 100)
			{ //8が含まれる場合
				pattern[i][6] += 8;
			}
			else
			{ //含まれない場合
				for (j = 0; j < 11; j++)
				{
					if (pattern[i][1] == pattern[j][1] && pattern[i][3] == 0 && pattern[j][3] == 0)
					{
						pattern[i][6] += 4;
					}
				}
			}
		}
		else
		{
			pattern[i][6] = -1;
		}
	}
	//最弱カードの評価値yの変化+followで出せない組の評価値yの変化
	if (pattern[11][0] >= 5)
	{
		if (state.rev == 0)
		{
			for (i = 0; i < 11; i++)
			{
				if (pattern[i][0] == 1)
				{
					pattern[i][6] -= 7;
					if (pattern[11][pattern[11][0] - 1] >= 100 || pattern[11][pattern[11][0] - 2] >= 100)
					{
						pattern[i][6] -= 8;
					}
				}
				else if (pattern[i][5] >= 97 && pattern[i][5] <= 100)
				{
					pattern[i][6] += 13;
				}
			}
		}
		else if (state.rev == 1)
		{
			for (i = 0; i < 11; i++)
			{
				if (pattern[i][0] == 13)
				{
					pattern[i][6] -= 7;
					if (pattern[11][pattern[11][0] - 1] >= 100 || pattern[11][pattern[11][0] - 2] >= 100)
					{
						pattern[i][6] -= 8;
					}
				}
				else if (pattern[i][5] >= 97 && pattern[i][5] <= 100)
				{
					pattern[i][6] += 13;
				}
			}
		}
	}
	//リスク(revxが高い組のみの場合,勝てる手札と判断しない)
	if (pattern[11][3] >= STRONG && pattern[11][3] <= 94)
	{
		j = 0;
		for (i = 0; i < 5; i++)
		{
			if (own_cards[6][i] > 0)
			{
				j++;
			}
		}
		if (j >= 4)
		{
			j = 202;
			for (i = 0; i < pattern[11][0]; i++)
			{
				if (j > pattern[i][5])
				{
					j = pattern[i][5];
				}
			}
			if (j > 50)
			{
				pattern[11][3] = STRONG - 1;
			}
		}
	}
	//2minによる評価値yの変化
	if (pattern[11][3] >= STRONG)
	{
		for (i = 0; i < 11; i++)
		{
			if (pattern[i][4] >= 1 && pattern[i][4] < STRONG)
			{
				pattern[i][6] = 2;
			}
			else if (pattern[i][3] == 14 && pattern[i][2] == 0)
			{
				if (pattern[i][4] == 100)
				{
					pattern[i][6] = 3;
				}
				else
				{
					pattern[i][6] = 1;
				}
			}
			if (pattern[i][4] >= STRONG)
			{
				if (pattern[11][3] >= 100)
				{
					pattern[i][6] = pattern[i][4] - 60;
					if (pattern[i][4] > 100)
					{
						pattern[i][6] = 41;
					}
				}
				else
				{
					pattern[i][6] = 110 - pattern[i][4];
					if (pattern[i][4] > 100)
					{
						pattern[i][6] = 9;
					}
				}
			}
		}
	}
	else if (pattern[11][6] >= STRONG && pattern[11][1] == 1)
	{
		pattern[11][1] = 0; //leadの判定変更
		if (pattern[i][1] >= 5 || (pattern[i][1] == 4 && (pattern[i][3] == 0 || pattern[i][3] == 14)))
		{ //革命手
			pattern[i][6] = 9;
		}
		else if (pattern[i][4] > 100)
		{ //8切り
			pattern[i][6] = 7;
		}
		else if (pattern[i][4] >= STRONG && pattern[i][4] > pattern[i][5])
		{ //x>=STRONG かつ x>revx
			pattern[i][6] = 110 - pattern[i][4];
		}
		else
		{
			pattern[i][6] = 8;
		}
	}
	if (pattern[11][0] >= 3 && pattern[pattern[11][0] - 1][3] == 14 && pattern[pattern[11][0] - 1][2] == 0)
	{
		pattern[11][0] -= 1; //ジョーカー単体は組数に含めない
	}
}

void value_strong(int pattern[][7], int own_cards[8][15], int used_cards[8][15], int rev, int n, int tes[3][4][3])
{ //強さ評価値と全体評価値計算
	//x,revx,z,revz,1min,2min
	//nはpatternへの格納位置(4か5)
	if (n != 4 && n != 5)
	{
		n = 5;
	}
	int i, j, k, jk, suit, t;
	int p = 0; //残り人数
	//ゲームの前後半を記録
	j = used_cards[5][1];
	if (j >= 0 && j < 13)
		t = 0;
	else if (j >= 13 && j < 32)
		t = 1;
	else if (j >= 32)
		t = 2;
	for (i = 0; i < 5; i++)
	{
		if (own_cards[6][i] > 0)
		{
			p++;
		}
	}
	int own_num[15] = {0}; //自分のカードの枚数を記録
	for (i = 1; i <= 13; i++)
	{
		own_num[i] = own_cards[0][i] + own_cards[1][i] + own_cards[2][i] + own_cards[3][i];
	}
	int max = 0;
	if (rev == 0)
	{
		for (i = 13; i > 0 && max == 0; i--)
		{
			if (own_num[i] + used_cards[4][i] != 4)
			{			 //同じ強さのカードが他の手札にあれば
				max = i; //その強さを記録
			}
		}
	}
	else
	{
		for (i = 1; i < 14 && max == 0; i++)
		{
			if (own_num[i] + used_cards[4][i] != 4)
			{			 //同じ強さのカードが他の手札にあれば
				max = i; //その強さを記録
			}
		}
	}
	if (max == 0)
	{
		if (rev == 0)
		{
			max = 1;
		}
		else
		{
			max = 13;
		}
	}

	for (i = 0; i < 11; i++)
	{
		if (pattern[i][3] != 14 && pattern[i][3] != 0)
		{
			//階段の場合
			if (rev == 0)
			{ //通常時
				pattern[i][n] = 100;
				for (j = pattern[i][0] + pattern[i][1]; j <= (max + 1); j++)
				{
					for (suit = 0; suit < 4; suit++)
					{
						jk = 1 - used_cards[4][14] - state.joker; //ジョーカーが含まれる可能性
						for (k = 0; k < pattern[i][1]; k++)
						{
							if (used_cards[suit][j - k] == 1 || own_cards[suit][j - k] == 1)
							{
								if (jk == 1)
								{
									jk = 0;
								}
								else
								{
									jk = 2;
								}
							}
						}
						if (jk != 2)
						{
							if (p == 2)
							{
								pattern[i][n] -= 15;
							}
							else if (jk == 0)
							{
								pattern[i][n] -= 1;
							}
							else
							{
								pattern[i][n] -= 6 - p;
							}
						}
					}
				}
				pattern[i][n] += tes[t][3][2] / 10;
			}
			else
			{ //革命時
				pattern[i][n] = 100;
				for (j = pattern[i][0] - (2 * pattern[i][1]) + 1; j >= (max - 1); j--)
				{ //j=カード強さ-(2*枚数)+1=上に出すときに必要な強さ(3に近い方)
					for (suit = 0; suit < 4; suit++)
					{
						jk = 1 - used_cards[4][14] - state.joker;
						for (k = 0; k < pattern[i][1]; k++)
						{
							if (used_cards[suit][j + k] == 1 || own_cards[suit][j + k] == 1)
							{
								if (jk == 1)
								{
									jk = 0;
								}
								else
								{
									jk = 2;
								}
							}
						}
						if (jk != 2)
						{
							if (p == 2)
							{
								pattern[i][n] -= 15;
							}
							else if (jk == 0)
							{
								pattern[i][n] -= 1;
							}
							else
							{
								pattern[i][n] -= 6 - p;
							}
						}
					}
				}
				pattern[i][n] += tes[t][3][2] / 10;
			}
			if (pattern[i][0] >= 6 && (pattern[i][0] - pattern[i][1] < 6))
			{ //8を含む場合
				pattern[i][n] += 100;
			}
		}
		else if (pattern[i][3] == 14 && pattern[i][2] == 0)
		{
			//ジョーカー単体
			if (used_cards[0][1] + own_cards[0][1] == 1)
			{ //スペード3の可能性なし
				pattern[i][n] = 100;
				pattern[i][n] += tes[t][0][2] / 10;
			}
			else
			{
				pattern[i][n] = 1;
			}
		}
		else if (pattern[i][0] == 0)
		{
			//カード無し
			pattern[i][0] = 0;
			pattern[i][1] = 0;
			pattern[i][2] = 0;
			pattern[i][3] = 0;
			pattern[i][n] = -1;
		}
		else
		{
			//枚数組
			pattern[i][n] = 100; //基準
			if (pattern[i][1] == 1)
			{
				//単体の場合、相手が出せる組があるとき-30する
				if (rev == 0)
				{
					for (j = max; j > pattern[i][0]; j--)
					{
						if ((4 - used_cards[4][j] - own_num[j]) >= 1)
						{
							pattern[i][n] -= 30;
						}
					}
				}
				else
				{
					for (j = max; j < pattern[i][0]; j++)
					{
						if ((4 - used_cards[4][j] - own_num[j]) >= 1)
						{
							pattern[i][n] -= 30;
						}
					}
				}
				if (used_cards[4][14] == 0 && state.joker == 0)
				{ //ジョーカーがある場合
					pattern[i][n] -= 1;
				}
				pattern[i][n] += tes[t][1][2] / 10;
			}
			else
			{
				//複数枚の場合
				if (rev == 0)
				{
					for (j = max; j > pattern[i][0]; j--)
					{
						if ((4 - used_cards[4][j] - own_num[j]) == (pattern[i][1] - 1) && used_cards[4][14] == 0 && state.joker == 0)
						{ //ジョーカーを含めると出される場合
							pattern[i][n] -= 1;
						}
						else if ((4 - used_cards[4][j] - own_num[j]) <= (pattern[i][1] - 1))
						{	//出される可能性なし
							//減らさない
						}
						else
						{
							k = (4 - used_cards[4][j] - own_num[j]) - pattern[i][1] - p + 5;
							if (p == 2)
							{
								pattern[i][n] -= 30;
							}
							else if (k <= 0)
							{
								pattern[i][n] -= 4;
							}
							else if (k == 1)
							{
								pattern[i][n] -= 9;
							}
							else if (k == 2)
							{
								pattern[i][n] -= 15;
							}
							else
							{
								pattern[i][n] -= 24;
							}
						}
					}
					pattern[i][n] += tes[t][2][2] / 10;
				}
				else
				{
					for (j = max; j < pattern[i][0]; j++)
					{
						if ((4 - used_cards[4][j] - own_num[j]) == (pattern[i][1] - 1) && used_cards[4][14] == 0 && state.joker == 0)
						{ //ジョーカーを含めると出される場合
							pattern[i][n] -= 1;
						}
						else if ((4 - used_cards[4][j] - own_num[j]) <= (pattern[i][1] - 1))
						{	//出される可能性なし
							//減らさない
						}
						else
						{
							k = (4 - used_cards[4][j] - own_num[j]) - pattern[i][1] - p + 5;
							if (p == 2)
							{
								pattern[i][n] -= 30;
							}
							else if (k <= 0)
							{
								pattern[i][n] -= 4;
							}
							else if (k == 1)
							{
								pattern[i][n] -= 9;
							}
							else if (k == 2)
							{
								pattern[i][n] -= 15;
							}
							else
							{
								pattern[i][n] -= 24;
							}
						}
					}
					pattern[i][n] += tes[t][2][2] / 10;
				}
			}
			if (pattern[i][n] <= 0)
			{
				pattern[i][n] = 1;
			}
			if (pattern[i][0] == 6)
			{ //8切り
				pattern[i][n] += 100;
			}
		}
	} //x計算ここまで

	for (i = 0; i < 11; i++)
	{
		if (n == 4)
		{ //1min,2minの決定
			if (i == 0)
			{
				pattern[11][2] = pattern[i][4];
				pattern[11][3] = 201;
			}
			else if (pattern[i][4] == -1 || (pattern[i][3] == 14 && pattern[i][2] == 0))
			{ //ジョーカー単体
			}
			else
			{
				if (pattern[i][4] < pattern[11][2])
				{
					pattern[11][3] = pattern[11][2];
					pattern[11][2] = pattern[i][4];
				}
				else if (pattern[i][4] < pattern[11][3])
				{
					pattern[11][3] = pattern[i][4];
				}
			}
		}
		if (n == 5)
		{ //rev2mの決定
			if (i == 0)
			{
				j = pattern[i][5];
				pattern[11][6] = 201;
			}
			else if (pattern[i][4] >= STRONG && pattern[i][4] > pattern[i][5])
			{ //x>=STRONG かつ x>revx
			}
			else if (pattern[i][1] >= 5 || (pattern[i][1] == 4 && (pattern[i][3] == 0 || pattern[i][3] == 14)))
			{ //革命手
			}
			else
			{
				if (pattern[i][5] < j)
				{
					pattern[11][6] = j;
					j = pattern[i][5];
				}
				else if (pattern[i][5] < pattern[11][6])
				{
					pattern[11][6] = pattern[i][5];
				}
			}
		}
		//z,revzの決定
		if (i == 0)
		{
			pattern[11][n] = 0;
		}
		if (i != 13)
		{
			if (pattern[i][1] <= 3 && pattern[i][1] != 0)
			{
				if (pattern[i][n] >= 1 && pattern[i][n] <= 30)
				{
					pattern[11][n] += 2;
				}
				else if (pattern[i][n] >= 31 && pattern[i][n] <= 60)
				{
					pattern[11][n] += 1;
				}
				else if (pattern[i][n] >= 61 && pattern[i][n] <= 90)
				{
					//pattern[11][n]+=0;
				}
				else if (pattern[i][n] >= 91 && pattern[i][n] <= 200)
				{
					if (rev == 0 && pattern[i][0] >= max)
					{ //一番強い数字
						pattern[11][n] -= pattern[i][1];
					}
					else if (rev == 1 && pattern[i][0] <= max)
					{
						pattern[11][n] -= pattern[i][1];
					}
					else
					{
						pattern[11][n] -= 1;
					}
				}
			}
			if (pattern[i][3] == 14 && pattern[i][2] == 0)
			{ //ジョーカー
				pattern[11][n] -= 2;
			}
		}
	}
}

void cardprint(int own_cards[8][15])
{
	int i, j;
	for (j = 0; j < 15; j++)
	{
		for (i = 0; i < 4; i++)
		{
			if (own_cards[i][j] >= 1)
			{
				printf(" ");
				if (own_cards[i][j] == 2)
				{
					printf("[jk]");
				}
				switch (i)
				{
				case 0:
					printf("s");
					break;
				case 1:
					printf("h");
					break;
				case 2:
					printf("d");
					break;
				case 3:
					printf("c");
					break;
				}
				if (j >= 1 && j <= 8)
					printf("%d", j + 2);
				else if (j == 9)
					printf("J");
				else if (j == 10)
					printf("Q");
				else if (j == 11)
					printf("K");
				else if (j == 12)
					printf("A");
				else if (j == 13)
					printf("2");
				else if (j == 0)
					printf("3-");
				else if (j == 14)
					printf("2+");
			}
		}
	}
	if (own_cards[4][1] == 2)
	{
		printf(" jk");
	}
	printf("\n");
}
void Tableprint(int own_cards[8][15], int print)
{
	//print 0:カードテーブルのみ 1:状態も表示
	int i, j, k;
	k = 5;
	if (print == 1)
	{
		k = 7;
	}
	fprintf(stderr, "    [3-][ 3][ 4][ 5][ 6][ 7][ 8][ 9][10][ J][ Q][ K][ A][ 2][2+]\n");
	for (i = 0; i < k; i++)
	{
		if (i == 0)
		{
			fprintf(stderr, "[ S]");
		}
		if (i == 1)
		{
			fprintf(stderr, "[ H]");
		}
		if (i == 2)
		{
			fprintf(stderr, "[ D]");
		}
		if (i == 3)
		{
			fprintf(stderr, "[ C]");
		}
		if (i == 4)
		{
			fprintf(stderr, "[jk]");
		}
		if (i == 5)
		{
			fprintf(stderr, "[ba]");
		}
		if (i == 6)
		{
			fprintf(stderr, "[ps]");
		}
		for (j = 0; j <= 14; j++)
		{
			if (own_cards[i][j] == 0)
			{
				fprintf(stderr, "    ");
			}
			else
			{
				fprintf(stderr, " %2d ", own_cards[i][j]);
			}
		}
		fprintf(stderr, "\n");
	}
}

//----------------------------------------------------------------------------------//
//----------------------------------------------------------------------------------//
//----------------------------------------------------------------------------------//
//----------------------------------------------------------------------------------//
//----------------------------------------------------------------------------------//
//----------------------------------------------------------------------------------//
//----------------------------------------------------------------------------------//
//----------------------------------------------------------------------------------//
//--------------------------------murata(senjutu == 0)------------------------------//
//----------------------------------------------------------------------------------//
//----------------------------------------------------------------------------------//
//----------------------------------------------------------------------------------//
//----------------------------------------------------------------------------------//
//----------------------------------------------------------------------------------//
//----------------------------------------------------------------------------------//
//----------------------------------------------------------------------------------//
//----------------------------------------------------------------------------------//
//----------------------------------------------------------------------------------//
//----------------------------------------------------------------------------------//
//----------------------------------------------------------------------------------//
//----------------------------------------------------------------------------------//
//----------------------------------------------------------------------------------//

//割り込み2000行付近

//-------------------------------------------------------------------------------------
//流れるかどうかをKou強さ評価値で判断モード→2
#define kou_e_value_mode 2
//残りクライアント数
#define remaining_client_num 3

//1で（１）のモード、２で（２）のモード
#define hand_num_switch 2
//（１）残ってるクライアントの中での最大手札
#define remaining_client_max_hand_num 6
//（２）前のクライアントの残り手札の上限
#define front_client_max_hand_num 6
//手札枚数を可変に(前のクライアントの階級が低いとき手札枚数緩和)（（２）のモードのみ）（１：可変、０：非可変）)
#define hand_variable 1

//流れるかどうか判断・単品縛り(9=J,10=Q,11=K)
#define ba_nagare_single_lock 11
//流れるかどうか判断・ペア縛り(6=8,7=9,8=10,9=J,10=Q,11=K)
#define ba_nagare_double_lock 8
//流れるかどうかの判断モード変更用（１：非可変、２：可変）
#define ba_nagare_mode 2
//場の状況、提出履歴、自分の手札　から現在の場において自分が最強のカードを持っているとき
//single 通常―＞４、緩和―＞３
//double 通常―＞3以上、緩和―＞
#define ba_nagare_max_limit_single 3
#define ba_nagare_max_limit_double 3

//前のクライアントをどこまで見るかのモード選択（１：1人前のみ、２：1人前と2人前）
#define front_client_mode 1

//前のクライアントの階級が平民であるとき起動しない（１で機能、０でそのまま）
#define front_client_rank 1

//空の時提出したものが自分の持っている最低ランクから
//2枚目以上のランクだった場合->2
//1枚目より大きい　かつ　2枚目より小さいランクだった場合->1
//1枚目以上の場合->3
#define front_client_log_st_off 3

//席順戦略を発動しないモード（１：パスしない、０：パスする）
#define st_no_log_mode 0
//-------------------------------------------------------------------------------------

//前のクライアントの履歴（この場の時このカード出した、パスした）から場を流した時に自分が持っているカードより弱いカードを提出するかの関数つくる
//	→それっぽい機能として前のクライアントが場が空の時に出したカードの強さによってパスするかどうかを変える
//	　ゲームごとに前のクライアントが場が空の状態で出した履歴を格納する(1枚、2枚それぞれ保存)
//	→空の状態の時に出したランクが自分の持っている最低ランク＋なんぼか　など機能発動の定義する！（強さ評価値使うのあり？）
//    空の状態の時に出したランクが自分の手札で必要ないものを消費出来そうにないならパスしない
//
//	空の時提出したものが自分の持っている最低ランクから2枚目より大きいのランクだった場合→パスしない

//前のクライアントの手札枚数のみ考慮
//	->前のクライアント（いい手札（流しやすい組）もってる）に対してのみ反応できる
//		->大富豪の確率が増える
//	->前のクライアントに対してのみ反応するので他のクライアントの手札枚数は無視
//		->貧民・大貧民の確率が増える（全部のクライアントの手札枚数を見ると負けにくくなる？）

//1000*10の試合回数では一定のログが取れない
// ->何回で落ち着いたデータになるか検証

//パスするほうが成績にバラツキがある→強かった時と弱かった時の差がある→差となる原因を見つける
//原因を探るのにどんなデータが必要？→仮説を立てて→検証

//バラつき原因
//
//仮説１：相手手札が強すぎる場合（次の場で自分が出せない）
//仮説２：相手が上がろうと出しに行くタイミングと、前のクライアントの残り手札上限がマッチしてない(front_client_max_hamd_num)
//　　　　相手の階級が高いほど上限枚数は多く設定すべき？（手札強いと上がる手札になるタイミングが早そう）→設定済み（タイミングは未検証）
//
//

//席順戦術(kou_follow_groupに介入しているので複数枚を出す時しか関与してない)
int seat_tactics(int own_cards[8][15], int my_player_num, int seat_order[5], int last_playnum, int used_cards[8][15], int game_count)
{
	////////////////////////////////////////////////////////
	//席順戦術を発動するとき返り値１、発動しない時返り値０//
	////////////////////////////////////////////////////////

	int i, j;

	if (remaining_client() <= remaining_client_num)
	{
		//クライアントremaining_client_num以下の時――起動しない
		seat_tactics_check_off++;
		return 0;
	}
	else if (remaining_client_max_hand(my_player_num) <= remaining_client_max_hand_num && hand_num_switch == 1)
	{
		//残ってるクライアントの手札枚数の最大値がremainig_client_max_hand_num以下の時――起動しない
		//要するにだれか上がりそうなときはやめとく
		seat_tactics_check_off++;
		return 0;
	}
	else if (front_client_hand_num_check(own_cards, my_player_num, seat_order, last_playnum) <= front_client_max_hand_num && hand_num_switch == 2)
	{
		//前のクライアントの手札枚数がfront_client_max_hand_num以下の時――起動しない
		seat_tactics_check_off++;
		return 0;
	}
	else if (seat_order_check(own_cards, my_player_num, seat_order, last_playnum) == 1)
	{
		if (kou_e_value_mode == 1)
		{
			//自分の流れそう関数
			if (ba_nagare(own_cards, seat_order, used_cards) == 1)
			{
				//場が流れそうなら
				if (front_client_log_st(own_cards, my_player_num, seat_order, last_playnum, used_cards, game_count) == 0)
				{
					//前のクライアントの提出ログから大丈夫そうなとき
					seat_tactics_check_on++;
					seat_tactics_check[game_count][0]++;
					seat_tactics_flag = 1;
					ba_nagare_last_playernum = last_playnum;

					if (st_no_log_mode == 1)
					{
						return 0;
					}
					else
					{
						return 1;
					}
				}
				else if (front_client_log_st(own_cards, my_player_num, seat_order, last_playnum, used_cards, game_count) == 1)
				{
					client_log_st_pass_off_count++;
				}
			}
		}
		else if (kou_e_value_mode == 2)
		{
			//kouの強さ評価値
			if (kou_e_value(own_cards, seat_order, used_cards) >= 86)
			{
				if (front_client_log_st(own_cards, my_player_num, seat_order, last_playnum, used_cards, game_count) == 0)
				{
					//前のクライアントの提出ログから大丈夫そうなとき
					seat_tactics_check_on++;
					seat_tactics_check[game_count][0]++;
					seat_tactics_flag = 1;
					ba_nagare_last_playernum = last_playnum;

					if (st_no_log_mode == 1)
					{
						return 0;
					}
					else
					{
						return 1;
					}
				}
				else if (front_client_log_st(own_cards, my_player_num, seat_order, last_playnum, used_cards, game_count) == 1)
				{
					client_log_st_pass_off_count++;
				}
			}
		}

		seat_tactics_check_off++;
		seat_tactics_flag = 0;
		return 0;
	}

	return 0;
}

//前のクライアントが空の場に提出した時のログを用いて、STを続行するか決定する
int front_client_log_st(int own_cards[8][15], int my_player_num, int seat_order[5], int last_playnum, int used_cards[8][15], int game_count)
{
	int i, j, count = 0, seat_order_remainig[5] = {5, 5, 5, 5, 5};

	for (i = 0; i < 5; i++)
	{
		//上がってないクライアントのみを格納していく
		if (state.player_qty[seat_order[i]] != 0)
		{
			seat_order_remainig[count] = seat_order[i];
			count++;
		}
	}

	int own_cards_limit_1 = 0, own_cards_limit_2 = 0, own_cards_limit_flag = 1, front_client_most_weak = 0, count_own_qty = 0, count_enemy_qty = 0;
	int continue_falg = 0;

	if (state.rev == 0)
	{
		//qyt==1
		for (j = 1; j <= 13; j++)
		{
			for (i = 0; i < 4; i++)
			{
				if (own_cards[i][j] == 1 && own_cards_limit_flag == 1)
				{
					own_cards_limit_1 = j;
					own_cards_limit_flag++;
					continue_falg++;
				}

				if (client_log[0][i][j][seat_order_remainig[count - 1]] == 1)
				{
					front_client_most_weak = j;
				}

				if (continue_falg == 1)
				{
					continue_falg--;
					continue;
				}

				if (own_cards[i][j] == 1 && own_cards_limit_flag == 2)
				{
					own_cards_limit_2 = j;
					own_cards_limit_flag++;
				}
			}

			if (own_cards_limit_flag >= 3)
			{
				break;
			}
		}

		if (front_client_log_st_off == 2)
		{
			if (own_cards_limit_2 < front_client_most_weak)
			{
				return 1;
			}
		}
		else if (front_client_log_st_off == 1)
		{
			if (own_cards_limit_2 > front_client_most_weak && own_cards_limit_1 < front_client_most_weak)
			{
				return 1;
			}
		}
		else if (front_client_log_st_off == 3)
		{
			if (own_cards_limit_1 <= front_client_most_weak)
			{
				if (own_cards_limit_1 != 0 && own_cards_limit_2 != 0 && front_client_most_weak != 0)
				{
					/*printf("*------------------*\n");
					printf("own_cards_limit_1 : %d\n", own_cards_limit_1);
					printf("own_cards_limit_2 : %d\n", own_cards_limit_2);
					printf("front_client_most_weak : %d\n", front_client_most_weak);
					printf("*------------------*\n\n");*/

					return 1;
				}
			}
		}

		continue_falg = 0;
		own_cards_limit_1 = 0;
		own_cards_limit_2 = 0;
		own_cards_limit_flag = 1;
		front_client_most_weak = 0;
		//qty=2
		for (j = 1; j <= 13; j++)
		{
			for (i = 0; i < 4; i++)
			{
				if (own_cards[i][j] == 1)
				{
					count_own_qty++;
				}
				if (client_log[1][i][j][seat_order_remainig[count - 1]] == 1)
				{
					count_enemy_qty++;
				}
			}

			if (count_own_qty >= 2 && own_cards_limit_flag == 1)
			{
				own_cards_limit_1 = j;
				own_cards_limit_flag++;
				continue_falg++;
			}

			if (count_enemy_qty >= 2)
			{
				front_client_most_weak = j;
			}

			if (continue_falg == 1)
			{
				continue_falg--;
				continue;
			}

			if (count_own_qty >= 2 && own_cards_limit_flag == 2)
			{
				own_cards_limit_2 = j;
				own_cards_limit_flag++;
			}

			if (own_cards_limit_flag >= 3)
			{
				break;
			}

			count_own_qty = 0;
			count_enemy_qty = 0;
		}
	}
	else if (state.rev == 1)
	{
		//qty=1
		for (j = 13; j >= 1; j--)
		{
			for (i = 0; i < 4; i++)
			{
				if (own_cards[i][j] == 1 && own_cards_limit_flag == 1)
				{
					own_cards_limit_1 = j;
					own_cards_limit_flag++;
					continue_falg++;
				}

				if (client_log[0][i][j][seat_order_remainig[count - 1]] == 1)
				{
					front_client_most_weak = j;
				}

				if (continue_falg == 1)
				{
					continue_falg--;
					continue;
				}

				if (own_cards[i][j] == 1 && own_cards_limit_flag == 2)
				{
					own_cards_limit_2 = j;
					own_cards_limit_flag++;
				}
			}

			if (own_cards_limit_flag >= 3)
			{
				break;
			}
		}

		if (front_client_log_st_off == 2)
		{
			if (own_cards_limit_2 < front_client_most_weak)
			{
				return 1;
			}
		}
		else if (front_client_log_st_off == 1)
		{
			if (own_cards_limit_2 > front_client_most_weak && own_cards_limit_1 < front_client_most_weak)
			{
				return 1;
			}
		}
		else if (front_client_log_st_off == 3)
		{
			if (own_cards_limit_1 <= front_client_most_weak)
			{
				if (own_cards_limit_1 != 0 && own_cards_limit_2 != 0 && front_client_most_weak != 0)
				{
					/*printf("*------------------*\n");
					printf("own_cards_limit_1 : %d\n", own_cards_limit_1);
					printf("own_cards_limit_2 : %d\n", own_cards_limit_2);
					printf("front_client_most_weak : %d\n", front_client_most_weak);
					printf("*------------------*\n\n");*/

					return 1;
				}
			}
		}

		continue_falg = 0;
		own_cards_limit_1 = 0;
		own_cards_limit_2 = 0;
		own_cards_limit_flag = 1;
		front_client_most_weak = 0;
		//qty=2
		for (j = 13; j >= 1; j--)
		{
			for (i = 0; i < 4; i++)
			{
				if (own_cards[i][j] == 1)
				{
					count_own_qty++;
				}
				if (client_log[1][i][j][seat_order_remainig[count - 1]] == 1)
				{
					count_enemy_qty++;
				}
			}

			if (count_own_qty >= 2 && own_cards_limit_flag == 1)
			{
				own_cards_limit_1 = j;
				own_cards_limit_flag++;
			}

			if (count_enemy_qty >= 2)
			{
				front_client_most_weak = j;
			}

			if (continue_falg == 1)
			{
				continue_falg--;
				continue;
			}

			if (count_own_qty >= 2 && own_cards_limit_flag == 2)
			{
				own_cards_limit_2 = j;
				own_cards_limit_flag++;
			}

			if (own_cards_limit_flag >= 3)
			{
				break;
			}

			count_own_qty = 0;
			count_enemy_qty = 0;
		}
	}

	if (front_client_log_st_off == 2)
	{
		if (own_cards_limit_2 < front_client_most_weak)
		{
			return 1;
		}
	}
	else if (front_client_log_st_off == 1)
	{
		if (own_cards_limit_2 > front_client_most_weak && own_cards_limit_1 < front_client_most_weak)
		{
			return 1;
		}
	}
	else if (front_client_log_st_off == 3)
	{
		if (own_cards_limit_1 <= front_client_most_weak)
		{
			if (own_cards_limit_1 != 0 && own_cards_limit_2 != 0 && front_client_most_weak != 0)
			{
				/*printf("*------------------*\n");
				printf("own_cards_limit_1 : %d\n", own_cards_limit_1);
				printf("own_cards_limit_2 : %d\n", own_cards_limit_2);
				printf("front_client_most_weak : %d\n", front_client_most_weak);
				printf("*------------------*\n\n");*/

				return 1;
			}
		}
	}

	/*printf("own_cards_limit_1 : %d\n", own_cards_limit_1);
	printf("own_cards_limit_2 : %d\n", own_cards_limit_2);
	printf("front_client_most_weak : %d\n\n", front_client_most_weak);*/

	//return 0でそのままパス、1でパスしない
	return 0;
}

//自分の前の席のクライアントが場に最後に出したとき、返り値１
int seat_order_check(int own_cards[8][15], int my_player_num, int seat_order[5], int last_playnum)
{
	//座ってる順番を格納する配列[->自分→〇→〇→〇→〇-]
	int i, count = 0, seat_order_remainig[5] = {5, 5, 5, 5, 5};

	for (i = 0; i < 5; i++)
	{
		//上がってないクライアントのみを格納していく
		if (state.player_qty[seat_order[i]] != 0)
		{
			seat_order_remainig[count] = seat_order[i];
			count++;
		}
	}

	if (front_client_rank == 1)
	{
		//前のクライアントの階級が平民の時起動しない
		if (seat_order_remainig[count - 1] == last_playnum && (state.player_rank[seat_order_remainig[count - 1]] == 2))
		{
			return 0;
		}
	}

	if (front_client_mode == 1)
	{
		//1人前のみ
		if (seat_order_remainig[count - 1] == last_playnum)
		{
			return 1;
		}
	}
	else if (front_client_mode == 2)
	{
		//1人前と2人前
		if (seat_order_remainig[count - 1] == last_playnum || seat_order_remainig[count - 2] == last_playnum)
		{
			return 1;
		}
	}

	return 0;
}

//場の状態からパスせざるを得ないとき返り値１
int pass_check(int own_cards[8][15], int ba_cards[8][15])
{
	if (state.qty == 1)
	{
		//単品
		if (pass_check_solo(own_cards, ba_cards) == 1)
		{
			return 0;
		}
	}
	else if (state.qty >= 2 && state.sequence == 0)
	{
		//複数枚（階段でない）
		if (pass_check_group(own_cards, ba_cards) == 1)
		{
			return 0;
		}
	}
	else if (state.qty >= 3 && state.sequence == 1)
	{
		//階段
		if (pass_check_kaidan(own_cards, ba_cards) == 1)
		{
			return 0;
		}
	}

	return 1;
}

//パスチェック＿単品（出すカードもってなければ返り値０）、（ジョーカー無考慮）
int pass_check_solo(int own_cards[8][15], int ba_cards[8][15])
{
	int i, j, lock_flag = 5, rev_flag = 0;

	//しばりなら、スートを格納
	if (state.lock == 1)
	{
		for (i = 0; i < 5; i++)
		{
			if (state.suit[i] == 1)
			{
				lock_flag = i;
			}
		}
	}
	//革命なら１
	if (state.rev == 1)
	{
		rev_flag = 1;
	}

	if (rev_flag == 0)
	{
		for (j = state.ord + 1; j <= 13; j++)
		{
			for (i = 0; i < 4; i++)
			{
				//しばりで持ってる
				if (lock_flag != 5)
				{
					if (own_cards[lock_flag][j] == 1)
					{
						return 1;
					}
					else
					{
						break;
					}
				}

				//普通に持ってる
				if (own_cards[i][j] == 1)
				{
					return 1;
				}
			}
		}
	}
	else if (rev_flag == 1)
	{
		for (j = state.ord - 1; j >= 1; j--)
		{
			for (i = 0; i < 5; i++)
			{
				//しばりで持ってる
				if (lock_flag != 5)
				{
					if (own_cards[lock_flag][j] == 1)
					{
						return 1;
					}
					else
					{
						break;
					}
				}

				//普通に持ってる
				if (own_cards[i][j] == 1)
				{
					return 1;
				}
			}
		}
	}

	return 0;
}

//パスチェック＿複数（階段でない）
int pass_check_group(int own_cards[8][15], int ba_cards[8][15])
{
	int i, j, k, lock_flag[4] = {5, 5, 5, 5}, lock_flag_count = 0, rev_flag = 0;
	int group_count = 0;

	//しばりなら、スートを格納
	if (state.lock == 1)
	{
		for (i = 0; i < 4; i++)
		{
			if (state.suit[i] == 1)
			{
				lock_flag[lock_flag_count] = i;
				lock_flag_count++;
			}
		}
	}
	//革命なら１
	if (state.rev == 1)
	{
		rev_flag = 1;
	}

	if (rev_flag == 0)
	{
		for (j = state.ord + 1; j <= 13; j++)
		{
			for (i = 0; i < 4; i++)
			{
				//しばりで持ってる
				if (state.lock == 1)
				{
					for (k = 0; k < lock_flag_count; k++)
					{
						if (lock_flag[k] = i)
						{
							if (own_cards[i][j] == 1)
							{
								group_count++;
								continue;
							}
						}
					}

					continue;
				}

				//持ってる
				if (own_cards[i][j] == 1)
				{
					group_count++;
				}
			}

			if (group_count == state.qty)
			{
				return 1;
			}
			else
			{
				group_count = 0;
			}
		}
	}
	else if (rev_flag == 1)
	{
		for (j = state.ord - 1; j >= 1; j--)
		{
			for (i = 0; i < 4; i++)
			{
				//しばりで持ってる
				if (state.lock == 1)
				{
					for (k = 0; k < lock_flag_count; k++)
					{
						if (lock_flag[k] = i)
						{
							if (own_cards[i][j] == 1)
							{
								group_count++;
								continue;
							}
						}
					}

					continue;
				}

				//持ってる
				if (own_cards[i][j] == 1)
				{
					group_count++;
				}
			}

			if (group_count == state.qty)
			{
				return 1;
			}
			else
			{
				group_count = 0;
			}
		}
	}

	return 0;
}

//パスチェック＿階段
int pass_check_kaidan(int own_cards[8][15], int ba_cards[8][15])
{
	int i, j, k, rev_flag = 0, qty_count = 0;
	int min_kaidan = 0, max_kaidan = 0;

	//革命なら１
	if (state.rev == 1)
	{
		rev_flag = 1;
	}

	//階段の最小ランクと最大ランクを格納
	for (i = 0; i < 4; i++)
	{
		for (j = 1; j <= 13; j++)
		{
			if (ba_cards[i][j] == 1 || ba_cards[i][j] == 2)
			{
				if (qty_count == 0)
				{
					//最小ランク
					min_kaidan = j;
				}
				else if (qty_count == (state.qty - 1))
				{
					//最大ランク
					max_kaidan = j;
				}

				qty_count++;
			}
		}
	}

	qty_count = 0;

	if (rev_flag == 0)
	{
		for (i = 0; i < 4; i++)
		{
			for (j = max_kaidan + 1; j <= 13; j++)
			{
				if (own_cards[i][j] == 1)
				{
					qty_count++;
				}
			}

			if (qty_count > state.qty)
			{
				return 1;
			}
			else
			{
				qty_count = 0;
			}
		}
	}
	else if (rev_flag == 1)
	{
		for (i = 0; i < 4; i++)
		{
			for (j = min_kaidan - 1; j >= 1; j--)
			{
				if (own_cards[i][j] == 1)
				{
					qty_count++;
				}
			}

			if (qty_count > state.qty)
			{
				return 1;
			}
			else
			{
				qty_count = 0;
			}
		}
	}

	return 0;
}

//残りクライアントの人数を返す(自分含め)
int remaining_client(void)
{
	int i, count = 0;

	for (i = 0; i < 5; i++)
	{
		//手札ない人カウント
		if (state.player_qty[i] == 0)
		{
			count++;
		}
	}

	if (status_print == 1)
	{
		printf("----------remaining_client:%d-----------\n", (5 - count));
	}

	return (5 - count);
}

//残っているクライアントの中で最大となる手札枚数を返す(自分含めず)
int remaining_client_max_hand(int my_player_num)
{
	int i, client_max_hand = 0;

	for (i = 0; i < 5; i++)
	{
		if (i == my_player_num)
		{
			continue;
		}
		else
		{
			//最大なら更新
			if (state.player_qty[i] > client_max_hand)
			{
				client_max_hand = state.player_qty[i];
			}
		}
	}

	if (status_print == 1)
	{
		printf("----------remaining_client_max_hand:%d-----------\n", client_max_hand);
	}

	return client_max_hand;
}

//前のクライアントの手札枚数を返す
int front_client_hand_num_check(int own_cards[8][15], int my_player_num, int seat_order[5], int last_playnum)
{
	//座ってる順番を格納する配列[->自分→〇→〇→〇→〇-]
	int i, count = 0, seat_order_remainig[5] = {5, 5, 5, 5, 5};

	for (i = 0; i < 5; i++)
	{
		//上がってないクライアントのみを格納していく
		if (state.player_qty[seat_order[i]] != 0)
		{
			seat_order_remainig[count] = seat_order[i];
			count++;
		}
	}

	if (hand_variable == 0)
	{
		return (state.player_qty[seat_order_remainig[count - 1]]);
	}
	else if (hand_variable == 1)
	{
		if (front_client_rank == 0)
		{
			//前が平民だとST起動しない―＞OFF
			if (state.player_rank[seat_order_remainig[count - 1]] == 2)
			{
				//もし前のクラインが平民だったら手札枚数制限を1枚緩和
				return (state.player_qty[seat_order_remainig[count - 1]] + 1);
			}
			else if (state.player_rank[seat_order_remainig[count - 1]] == 3 || state.player_rank[seat_order_remainig[count - 1]] == 4)
			{
				//もし前のクラインが貧民or大貧民だったら手札枚数制限を２枚緩和
				return (state.player_qty[seat_order_remainig[count - 1]] + 2);
			}
		}
		else if (front_client_rank == 1)
		{
			//前のクラインが平民だとST起動しない→ONのとき
			if (state.player_rank[seat_order_remainig[count - 1]] == 3)
			{
				//もし前のクラインが貧民だったら手札枚数制限を1枚緩和
				return (state.player_qty[seat_order_remainig[count - 1]] + 1);
			}
			else if (state.player_rank[seat_order_remainig[count - 1]] == 4)
			{
				//もし前のクラインが大貧民だったら手札枚数制限を２枚緩和
				return (state.player_qty[seat_order_remainig[count - 1]] + 2);
			}
		}
	}

	return (state.player_qty[seat_order_remainig[count - 1]]);
}

//残っているクライアントの中で最小となる手札枚数を返す(自分含めず)
int remaining_client_min_hand(int my_player_num)
{
	int i, client_min_hand = 0, flag = 0;

	for (i = 0; i < 5; i++)
	{
		if (i == my_player_num)
		{
			continue;
		}
		else
		{
			//最大なら更新
			if (flag == 0)
			{
				flag = 1;
				client_min_hand = state.player_qty[i];
			}

			if (state.player_qty[i] < client_min_hand)
			{
				client_min_hand = state.player_qty[i];
			}
		}
	}

	if (status_print == 1)
	{
		printf("----------remaining_client_max_hand:%d-----------\n", client_min_hand);
	}

	return client_min_hand;
}

//場が流れそうなら１、流れそうにないなら０
int ba_nagare(int own_cards[8][15], int seat_order[5], int used_cards[8][15])
{
	int i, j;

	//自分の前の席の機能入れる

	if (ba_nagare_max(own_cards, used_cards) >= 1)
	{
		//自分が場における最強のカード持ってる
		if (status_print == 1)
		{
			printf("----------ba_nagare_max <ON>-----------\n");
		}
		ba_nagare_max_count++;
		return 1;
	}

	if (ba_nagare_mode == 1)
	{
		if (state.rev == 0)
		{
			//革命でない
			if (state.ord >= ba_nagare_single_lock && state.qty == 1 && state.lock == 1)
			{
				//単体しばりで流れそうか判断
				if (status_print == 1)
				{
					printf("----------ba_nagare_single_lock <ON>-----------\n");
				}
				ba_nagare_single++;
				return 1;
			}
			else if (state.ord >= ba_nagare_double_lock && state.qty == 2 && state.lock == 1)
			{
				//ペアしばりで流れそうか判断
				if (status_print == 1)
				{
					printf("----------ba_nagare_double_lock <ON>-----------\n");
				}
				ba_nagare_double++;
				return 1;
			}
			else if (state.qty >= 3)
			{
				//3カード以上の組（3ガード・階段など）は流れそう
				if (status_print == 1)
				{
					printf("----------ba_nagare_three_card_or_kaidan <ON>-----------\n");
				}
				ba_nagare_three_cards++;
				return 1;
			}
		}
		else if (state.rev == 1)
		{
			//革命時
			if (state.ord <= (ba_nagare_single_lock - 8) && state.qty == 1 && state.lock == 1)
			{
				//単体しばりで流れそうか判断
				if (status_print == 1)
				{
					printf("----------ba_nagare_single_lock <ON>-----------\n");
				}
				ba_nagare_single++;
				return 1;
			}
			else if (state.ord <= (ba_nagare_double_lock - 4) && state.qty == 2 && state.lock == 1)
			{
				//ペアしばりで流れそうか判断
				if (status_print == 1)
				{
					printf("----------ba_nagare_double_lock <ON>-----------\n");
				}
				ba_nagare_double++;
				return 1;
			}
			else if (state.qty >= 3)
			{
				//3カード以上の組（3ガード・階段など）は流れそう
				if (status_print == 1)
				{
					printf("----------ba_nagare_three_card_or_kaidan <ON>-----------\n");
				}
				ba_nagare_three_cards++;
				return 1;
			}
		}
	}
	else if (ba_nagare_mode == 2)
	{
		//場が流れる可変モード（こっちは関数内で革命の分岐を行っている）

		if (state.qty == 1 && state.lock == 1)
		{
			if (ba_nagare_single_variable(own_cards, seat_order, used_cards) == 1)
			{
				if (status_print == 1)
				{
					printf("----------ba_nagare_single_lock <ON>-----------\n");
				}
				ba_nagare_single++;
				return 1;
			}
		}
		else if (state.qty == 2 && state.lock == 1)
		{
			if (ba_nagare_double_variable(own_cards, seat_order, used_cards) == 1)
			{
				if (status_print == 1)
				{
					printf("----------ba_nagare_double_lock <ON>-----------\n");
				}
				ba_nagare_double++;
				return 1;
			}
		}
		else if (state.qty >= 3)
		{
			//3カード以上の組（3ガード・階段など）は流れそう
			if (status_print == 1)
			{
				printf("----------ba_nagare_three_card_or_kaidan <ON>-----------\n");
			}
			ba_nagare_three_cards++;
			return 1;
		}
	}

	if (status_print == 1)
	{
		printf("----------ba_nagare <OFF>-----------\n");
	}
	return 0;
}

//流れるかどうかの判断の基準（define）を可変に（単品）
int ba_nagare_single_variable(int own_cards[8][15], int seat_order[5], int used_cards[8][15])
{
	int ba_nagare_single_lock_variable = ba_nagare_single_lock;
	float a, b;

	a = now_cards_ord_used(used_cards, own_cards); //現在場に出ているカードより強いランクのカードの未提出総数外(使われたやつと自分の)
	b = now_cards_ord(used_cards);				   //現在場に出ているカードより強いランクのカードの総数

	//a / bは初めはかなり小さい数字、後半ほど大きくなる(値域:0~1)

	if (b != 0)
	{
		if (state.rev == 0)
		{
			if (a / b < 0.25)
			{
				if (state.ord >= ba_nagare_single_lock_variable)
				{
					return 1;
				}
			}
			else if (a / b < 0.33)
			{
				if (state.ord >= ba_nagare_single_lock_variable - 1)
				{
					return 1;
				}
			}
			else if (a / b < 0.5)
			{
				if (state.ord >= ba_nagare_single_lock_variable - 2)
				{
					return 1;
				}
			}
			else
			{
				if (state.ord >= ba_nagare_single_lock_variable - 3)
				{
					return 1;
				}
			}
		}
		else if (state.rev == 1)
		{
			if (a / b < 0.25)
			{
				if (state.ord <= ba_nagare_single_lock_variable)
				{
					return 1;
				}
			}
			else if (a / b < 0.33)
			{
				if (state.ord <= ba_nagare_single_lock_variable - 1)
				{
					return 1;
				}
			}
			else if (a / b < 0.5)
			{
				if (state.ord <= ba_nagare_single_lock_variable - 2)
				{
					return 1;
				}
			}
			else
			{
				if (state.ord <= ba_nagare_single_lock_variable - 3)
				{
					return 1;
				}
			}
		}
	}

	return 0;
}

//流れるかどうかの判断の基準（define）を可変に（ペア）
int ba_nagare_double_variable(int own_cards[8][15], int seat_order[5], int used_cards[8][15])
{
	int ba_nagare_double_lock_variable = ba_nagare_double_lock;
	int a, b;

	a = now_cards_ord_used(used_cards, own_cards);
	b = now_cards_ord(used_cards);

	if (b != 0)
	{
		if (state.rev == 0)
		{
			if (a / b < 0.25)
			{
				if (state.ord >= ba_nagare_double_lock_variable)
				{
					return 1;
				}
			}
			else if (a / b < 0.33)
			{
				if (state.ord >= ba_nagare_double_lock_variable - 1)
				{
					return 1;
				}
			}
			else if (a / b < 0.5)
			{
				if (state.ord >= ba_nagare_double_lock_variable - 2)
				{
					return 1;
				}
			}
			else
			{
				if (state.ord >= ba_nagare_double_lock_variable - 3)
				{
					return 1;
				}
			}
		}
		else if (state.rev == 0)
		{
			if (a / b < 0.25)
			{
				if (state.ord <= ba_nagare_double_lock_variable)
				{
					return 1;
				}
			}
			else if (a / b < 0.33)
			{
				if (state.ord <= ba_nagare_double_lock_variable - 1)
				{
					return 1;
				}
			}
			else if (a / b < 0.5)
			{
				if (state.ord <= ba_nagare_double_lock_variable - 2)
				{
					return 1;
				}
			}
			else
			{
				if (state.ord <= ba_nagare_double_lock_variable - 3)
				{
					return 1;
				}
			}
		}
	}

	return 0;
}

//現在場に出ているカードより強いランクのカードの内，未提出カードの合計
int now_cards_ord_used(int used_cards[8][15], int own_cards[8][15])
{
	int i, j, count = 0;

	if (state.rev == 0)
	{
		for (i = 0; i < 4; i++)
		{
			for (j = state.ord + 1; j <= 13; j++)
			{
				if (used_cards[i][j] != 1 && own_cards[i][j] != 1)
				{
					count++;
				}
			}
		}
	}
	else if (state.rev == 1)
	{
		for (i = 0; i < 4; i++)
		{
			for (j = state.ord - 1; j >= 1; j--)
			{
				if (used_cards[i][j] != 1 && own_cards[i][j] != 1)
				{
					count++;
				}
			}
		}
	}

	return count;
}

//現在場に出ているカードより強いランクのカードの総数を返す
int now_cards_ord(int used_cards[8][15])
{
	int i, j, count = 0;

	if (state.rev == 0)
	{
		for (i = 0; i < 4; i++)
		{
			for (j = state.ord + 1; j <= 13; j++)
			{
				count++;
			}
		}
	}
	else if (state.rev == 1)
	{
		for (i = 0; i < 4; i++)
		{
			for (j = state.ord - 1; j >= 1; j--)
			{
				count++;
			}
		}
	}

	return count;
}

//場の状況、提出履歴、自分の手札　から現在の場において自分が最強のカードを持っているとき
int ba_nagare_max(int own_cards[8][15], int used_cards[8][15])
{
	int ba_nagare_max_single_buf = 0, ba_nagare_max_double_buf = 0;

	if (state.qty == 1)
	{
		ba_nagare_max_single_buf = ba_nagare_max_single(own_cards, used_cards);
		if (ba_nagare_max_single_buf >= 1)
		{
			//場が一枚で最強持ってる
			ba_nagare_max_count_single++;
			return ba_nagare_max_single_buf;
		}
	}
	else if (state.qty == 2)
	{
		ba_nagare_max_double_buf = ba_nagare_max_double(own_cards, used_cards);
		if (ba_nagare_max_double_buf >= 1)
		{
			//場が二枚で最強持ってる
			ba_nagare_max_count_double++;
			return ba_nagare_max_double_buf;
		}
	}

	return 0;
}

//場が一枚（返り値はランク［1～13］）
int ba_nagare_max_single(int own_cards[8][15], int used_cards[8][15])
{
	int i, j;
	int rank_count_used_cards = 0, rank_count_own_cards = 0, ba_suit;
	int flag = 0;

	if (state.rev == 0)
	{
		//革命でない
		if (state.lock == 0)
		{
			//縛りない

			//上から(その時に、最も強い奴から)
			for (j = 13; j > state.ord; j--)
			{
				//ランクのループ
				for (i = 0; i < 4; i++)
				{
					//スートのループ
					if (used_cards[i][j] == 1)
					{
						//そのランクの提出済み枚数を調べる
						rank_count_used_cards++;
					}

					if (own_cards[i][j] == 1)
					{
						//そのランクの自分の持ってる枚数を調べる
						rank_count_own_cards++;
					}
				}

				if (rank_count_used_cards == 4)
				{
					//全て提出済みのランクなら次へ
					rank_count_used_cards = 0;
					rank_count_own_cards = 0;
					continue;
				}
				else if (rank_count_used_cards + rank_count_own_cards >= ba_nagare_max_limit_single)
				{
					//自分のみが、このランクのカードを持っているとき(上から順番に見てきた)
					flag = j;
					rank_count_used_cards = 0;
					rank_count_own_cards = 0;
					break;
				}
				else
				{
					return 0;
				}
			}

			//下から(その時に、場より一つ強いとこから)
			for (j = state.ord + 1; j <= flag; j++)
			{
				//ランクのループ
				for (i = 0; i < 4; i++)
				{
					//スートのループ
					if (used_cards[i][j] == 1)
					{
						//そのランクの提出済み枚数を調べる
						rank_count_used_cards++;
					}

					if (own_cards[i][j] == 1)
					{
						//そのランクの自分の持ってる枚数を調べる
						rank_count_own_cards++;
					}
				}

				if (rank_count_used_cards == 4)
				{
					//全て提出済みのランクなら次へ
					rank_count_used_cards = 0;
					rank_count_own_cards = 0;
					continue;
				}
				else if (rank_count_used_cards + rank_count_own_cards >= ba_nagare_max_limit_single)
				{
					//自分のみが、このランクのカードを持っているとき(下から順番に見てきた)
					if (j == flag)
					{
						ba_nagare_max_count_single_nomal++;
						return j;
					}
					else
					{
						rank_count_used_cards = 0;
						rank_count_own_cards = 0;
						continue;
					}
				}
				else
				{
					return 0;
				}
			}

			return 0;
		}
		else
		{
			//縛りあり

			//場のスートを調べる
			for (i = 0; i < 4; i++)
			{
				if (state.suit[i] == 1)
				{
					//場のスートをba_suitに格納
					ba_suit = i;
				}
			}

			//上から
			for (j = 13; j > state.ord; j--)
			{
				//ランクのループ
				if (used_cards[ba_suit][j] == 0 && own_cards[ba_suit][j] == 1)
				{
					//提出済みでない　かつ　自分が持ってるとき
					flag = j;
				}
				else if (used_cards[ba_suit][j] == 0 && own_cards[ba_suit][j] == 0)
				{
					//提出済みでない　かつ　自分が持ってない
					return 0;
				}
			}

			//下から
			for (j = state.ord + 1; j <= flag; j++)
			{
				//ランクのループ
				if (used_cards[ba_suit][j] == 0 && own_cards[ba_suit][j] == 1)
				{
					//提出済みでない　かつ　自分が持ってるとき
					if (j == flag)
					{
						ba_nagare_max_count_single_sibari++;
						return j;
					}
					else
					{
						continue;
					}
				}
				else if (used_cards[ba_suit][j] == 0 && own_cards[ba_suit][j] == 0)
				{
					//提出済みでない　かつ　自分が持ってない
					return 0;
				}
			}

			return 0;
		}
	}
	else
	{
		//革命時
		if (state.lock == 0)
		{
			//縛りない

			//上から(その時に、最も強い奴から)
			for (j = 1; j < state.ord; j++)
			{
				//ランクのループ
				for (i = 0; i < 4; i++)
				{
					//スートのループ
					if (used_cards[i][j] == 1)
					{
						//そのランクの提出済み枚数を調べる
						rank_count_used_cards++;
					}

					if (own_cards[i][j] == 1)
					{
						//そのランクの自分の持ってる枚数を調べる
						rank_count_own_cards++;
					}
				}

				if (rank_count_used_cards == 4)
				{
					//全て提出済みのランクなら次へ
					rank_count_used_cards = 0;
					rank_count_own_cards = 0;
					continue;
				}
				else if (rank_count_used_cards + rank_count_own_cards >= ba_nagare_max_limit_single)
				{
					//自分のみが、このランクのカードを持っているとき(上から順番に見てきた)
					flag = j;
					rank_count_used_cards = 0;
					rank_count_own_cards = 0;
					break;
				}
				else
				{
					return 0;
				}
			}

			//下から(その時に、場より一つ強いとこから)
			for (j = state.ord - 1; j >= flag; j--)
			{
				//ランクのループ
				for (i = 0; i < 4; i++)
				{
					//スートのループ
					if (used_cards[i][j] == 1)
					{
						//そのランクの提出済み枚数を調べる
						rank_count_used_cards++;
					}

					if (own_cards[i][j] == 1)
					{
						//そのランクの自分の持ってる枚数を調べる
						rank_count_own_cards++;
					}
				}

				if (rank_count_used_cards == 4)
				{
					//全て提出済みのランクなら次へ
					rank_count_used_cards = 0;
					rank_count_own_cards = 0;
					continue;
				}
				else if (rank_count_used_cards + rank_count_own_cards >= ba_nagare_max_limit_single)
				{
					//自分のみが、このランクのカードを持っているとき(下から順番に見てきた)
					if (j == flag)
					{
						ba_nagare_max_count_single_nomal_rev++;
						return j;
					}
					else
					{
						rank_count_used_cards = 0;
						rank_count_own_cards = 0;
						continue;
					}
				}
				else
				{
					return 0;
				}
			}

			return 0;
		}
		else
		{
			//縛りあり

			//場のスートを調べる
			for (i = 0; i < 4; i++)
			{
				if (state.suit[i] == 1)
				{
					//場のスートをba_suitに格納
					ba_suit = i;
				}
			}

			//上から
			for (j = 1; j < state.ord; j++)
			{
				//ランクのループ
				if (used_cards[ba_suit][j] == 0 && own_cards[ba_suit][j] == 1)
				{
					//提出済みでない　かつ　自分が持ってるとき
					flag = j;
				}
				else if (used_cards[ba_suit][j] == 0 && own_cards[ba_suit][j] == 0)
				{
					//提出済みでない　かつ　自分が持ってない
					return 0;
				}
			}

			//下から
			for (j = state.ord - 1; j >= flag; j--)
			{
				//ランクのループ
				if (used_cards[ba_suit][j] == 0 && own_cards[ba_suit][j] == 1)
				{
					//提出済みでない　かつ　自分が持ってるとき
					if (j == flag)
					{
						ba_nagare_max_count_single_sibari_rev++;
						return j;
					}
					else
					{
						continue;
					}
				}
				else if (used_cards[ba_suit][j] == 0 && own_cards[ba_suit][j] == 0)
				{
					//提出済みでない　かつ　自分が持ってない
					return 0;
				}
			}

			return 0;
		}
	}

	return 0;
}

//場が二枚（返り値はランク［1～13］）
int ba_nagare_max_double(int own_cards[8][15], int used_cards[8][15])
{
	int i, j;
	int rank_count_used_cards = 0, rank_count_own_cards = 0, ba_suit[2];
	int flag = 0, suit_count = 0;

	if (state.rev == 0)
	{
		//革命でない
		if (state.lock == 0)
		{
			//縛りない

			//上から(その時に、最も強い奴から)
			for (j = 13; j > state.ord; j--)
			{
				//ランクのループ
				for (i = 0; i < 4; i++)
				{
					//スートのループ
					if (used_cards[i][j] == 1)
					{
						//そのランクの提出済み枚数を調べる
						rank_count_used_cards++;
					}

					if (own_cards[i][j] == 1)
					{
						//そのランクの自分の持ってる枚数を調べる
						rank_count_own_cards++;
					}
				}

				if (rank_count_used_cards >= 3)
				{
					//提出済みカードが3枚以上（３か４）の時（このランクは誰も出せない）
					rank_count_used_cards = 0;
					rank_count_own_cards = 0;
					continue;
				}
				else if (rank_count_used_cards + rank_count_own_cards >= ba_nagare_max_limit_double)
				{
					//自分のみが、このランクのカードを持っているとき(上から順番に見てきた)
					flag = j;
					rank_count_used_cards = 0;
					rank_count_own_cards = 0;
					break;
				}
				else
				{
					//相手が持ってる
					return 0;
				}
			}

			//下から(その時に、場より一つ強いとこから)
			for (j = state.ord + 1; j <= flag; j++)
			{
				//ランクのループ
				for (i = 0; i < 4; i++)
				{
					//スートのループ
					if (used_cards[i][j] == 1)
					{
						//そのランクの提出済み枚数を調べる
						rank_count_used_cards++;
					}

					if (own_cards[i][j] == 1)
					{
						//そのランクの自分の持ってる枚数を調べる
						rank_count_own_cards++;
					}
				}

				if (rank_count_used_cards >= 3)
				{
					//提出済みカードが3枚以上（３か４）の時
					rank_count_used_cards = 0;
					rank_count_own_cards = 0;
					continue;
				}
				else if (rank_count_used_cards + rank_count_own_cards >= ba_nagare_max_limit_double)
				{
					//自分のみが、このランクのカードを持っているとき(下から順番に見てきた)
					if (j == flag)
					{
						ba_nagare_max_count_double_nomal++;
						return j;
					}
					else
					{
						rank_count_used_cards = 0;
						rank_count_own_cards = 0;
						continue;
					}
				}
				else
				{
					return 0;
				}
			}

			return 0;
		}
		else
		{
			//縛りあり

			//場のスートを調べる
			for (i = 0; i < 4; i++)
			{
				if (state.suit[i] == 1)
				{
					//場のスートをba_suitに格納
					ba_suit[suit_count] = i;
					suit_count++;
				}
			}

			//上から
			for (j = 13; j > state.ord; j--)
			{
				//ランクのループ
				if (used_cards[(ba_suit[0])][j] == 0 && used_cards[ba_suit[1]][j] == 0)
				{
					//ペアのスートの両方が出てないとき
					if (own_cards[ba_suit[0]][j] == 0 && own_cards[ba_suit[1]][j] == 0)
					{
						//かつ　ペアのスートの両方を持ってるとき
						flag = j;
					}
					else if (own_cards[ba_suit[0]][j] == 1 || own_cards[ba_suit[1]][j] == 1)
					{
						//かつ　ペアのスートを一方でも持ってないとき
						return 0;
					}
				}
			}

			//下から
			for (j = state.ord + 1; j <= flag; j++)
			{
				//ランクのループ
				if (used_cards[ba_suit[0]][j] == 0 && used_cards[ba_suit[1]][j] == 0)
				{
					//ペアのスートの両方が出てないとき
					if (own_cards[ba_suit[0]][j] == 0 && own_cards[ba_suit[1]][j] == 0)
					{
						//かつ　ペアのスートの両方を持ってるとき
						if (j == flag)
						{
							ba_nagare_max_count_double_sibari++;
							return j;
						}
						else
						{
							continue;
						}
					}
					else if (own_cards[ba_suit[0]][j] == 1 || own_cards[ba_suit[1]][j] == 1)
					{
						//かつ　ペアのスートを一方でも持ってないとき
						return 0;
					}
				}
			}

			return 0;
		}
	}
	else
	{
		//革命時
		if (state.lock == 0)
		{
			//縛りない

			//上から(その時に、最も強い奴から)
			for (j = 1; j < state.ord; j++)
			{
				//ランクのループ
				for (i = 0; i < 4; i++)
				{
					//スートのループ
					if (used_cards[i][j] == 1)
					{
						//そのランクの提出済み枚数を調べる
						rank_count_used_cards++;
					}

					if (own_cards[i][j] == 1)
					{
						//そのランクの自分の持ってる枚数を調べる
						rank_count_own_cards++;
					}
				}

				if (rank_count_used_cards >= 3)
				{
					//提出済みカードが3枚以上（３か４）の時
					rank_count_used_cards = 0;
					rank_count_own_cards = 0;
					continue;
				}
				else if (rank_count_used_cards + rank_count_own_cards >= ba_nagare_max_limit_double)
				{
					//自分のみが、このランクのカードを持っているとき(上から順番に見てきた)
					flag = j;
					rank_count_used_cards = 0;
					rank_count_own_cards = 0;
					break;
				}
				else
				{
					return 0;
				}
			}

			//下から(その時に、場より一つ強いとこから)
			for (j = state.ord - 1; j >= flag; j--)
			{
				//ランクのループ
				for (i = 0; i < 4; i++)
				{
					//スートのループ
					if (used_cards[i][j] == 1)
					{
						//そのランクの提出済み枚数を調べる
						rank_count_used_cards++;
					}

					if (own_cards[i][j] == 1)
					{
						//そのランクの自分の持ってる枚数を調べる
						rank_count_own_cards++;
					}
				}

				if (rank_count_used_cards >= 3)
				{
					//提出済みカードが3枚以上（３か４）の時
					rank_count_used_cards = 0;
					rank_count_own_cards = 0;
					continue;
				}
				else if (rank_count_used_cards + rank_count_own_cards >= ba_nagare_max_limit_double)
				{
					//自分のみが、このランクのカードを持っているとき(下から順番に見てきた)
					if (j == flag)
					{
						ba_nagare_max_count_double_nomal_rev++;
						return j;
					}
					else
					{
						rank_count_used_cards = 0;
						rank_count_own_cards = 0;
						continue;
					}
				}
				else
				{
					return 0;
				}
			}

			return 0;
		}
		else
		{
			//縛りあり

			//場のスートを調べる
			for (i = 0; i < 4; i++)
			{
				if (state.suit[i] == 1)
				{
					//場のスートをba_suitに格納
					ba_suit[suit_count] = i;
					suit_count++;
				}
			}

			//上から
			for (j = 1; j < state.ord; j++)
			{
				//ランクのループ
				if (used_cards[ba_suit[0]][j] == 0 && used_cards[ba_suit[1]][j] == 0)
				{
					//ペアのスートの両方が出てないとき
					if (own_cards[ba_suit[0]][j] == 0 && own_cards[ba_suit[1]][j] == 0)
					{
						//かつ　ペアのスートの両方を持ってるとき
						flag = j;
					}
					else if (own_cards[ba_suit[0]][j] == 1 || own_cards[ba_suit[1]][j] == 1)
					{
						//かつ　ペアのスートを一方でも持ってないとき
						return 0;
					}
				}
			}

			//下から
			for (j = state.ord - 1; j >= flag; j--)
			{
				//ランクのループ
				if (used_cards[ba_suit[0]][j] == 0 && used_cards[ba_suit[1]][j] == 0)
				{
					//ペアのスートの両方が出てないとき
					if (own_cards[ba_suit[0]][j] == 0 && own_cards[ba_suit[1]][j] == 0)
					{
						//かつ　ペアのスートの両方を持ってるとき
						if (j == flag)
						{
							ba_nagare_max_count_double_sibari_rev++;
							return j;
						}
						else
						{
							continue;
						}
					}
					else if (own_cards[ba_suit[0]][j] == 1 || own_cards[ba_suit[1]][j] == 1)
					{
						//かつ　ペアのスートを一方でも持ってないとき
						return 0;
					}
				}
			}

			return 0;
		}
	}

	return 0;
}

//kouの強さ評価値計算
int kou_e_value(int own_cards[8][15], int seat_order[5], int used_cards[8][15])
{
	int i, j, count = 0;
	//qty=1
	int n = 0, c = 0;
	//qty>=2
	int r[15] = {0}, r_count = 0, fn[15] = {15}, player = 0, e = 100;

	if (state.qty == 1)
	{
		//nの値計算
		if (state.rev == 0)
		{
			for (j = state.ord + 1; j <= 13; j++)
			{
				for (i = 0; i < 4; i++)
				{
					if (own_cards[i][j] != 1 && used_cards[i][j] != 1)
					{
						n++;
						break;
					}
				}
			}
		}
		else if (state.rev == 1)
		{
			for (j = state.ord - 1; j >= 1; j--)
			{
				for (i = 0; i < 4; i++)
				{
					if (own_cards[i][j] != 1 && used_cards[i][j] != 1)
					{
						n++;
						break;
					}
				}
			}
		}

		//ｃの値計算（ジョーカーがまだ出てない　かつ　自分が持っていない場合１　、　そうでない場合０）
		if (used_cards[4][1] != 2 && state.joker == 0)
		{
			c = 1;
		}
		else
		{
			c = 0;
		}

		//強さ評価値E決定
		return (100 - (n * 30) - c);
	}
	else if (state.qty >= 2 && state.sequence == 0)
	{
		//n_iの計算準備
		if (state.rev == 0)
		{
			for (j = state.ord + 1; j <= 13; j++)
			{
				for (i = 0; i < 4; i++)
				{
					if (own_cards[i][j] != 1 && used_cards[i][j] != 1)
					{
						count++;
					}
				}

				if (count >= state.qty)
				{
					r[r_count] = count;
					r_count++;
				}
				count = 0;
			}
		}
		else if (state.rev == 1)
		{
			for (j = state.ord - 1; j >= 1; j--)
			{
				for (i = 0; i < 4; i++)
				{
					if (own_cards[i][j] != 1 && used_cards[i][j] != 1)
					{
						count++;
					}
				}

				if (count >= state.qty)
				{
					r[r_count] = count;
					r_count++;
				}
				count = 0;
			}
		}

		//残りプレイヤ数
		for (i = 0; i < 5; i++)
		{
			if (state.player_qty > 0)
			{
				player++;
			}
		}

		//f(n_i)計算
		for (i = 0; i < r_count; i++)
		{
			if ((r[i] - state.qty - player + 5) == 0)
			{
				fn[i] = 4;
			}
			else if ((r[i] - state.qty - player + 5) == 1)
			{
				fn[i] = 9;
			}
			else if ((r[i] - state.qty - player + 5) == 2)
			{
				fn[i] = 15;
			}
			else if ((r[i] - state.qty - player + 5) >= 3)
			{
				fn[i] = 24;
			}
		}

		//Σ処理
		for (i = 0; i < r_count; i++)
		{
			e = e - fn[i];
		}

		//強さ評価値E決定
		return e;
	}

	return 0;
}

/*struct state_type
{
    int ord;            // 現在場に出ているカードの強さ(カードの３＝１、カードの２＝１３)
    int sequence;       // 場に出ているカードが階段なら1、枚数組なら0
    int qty;            // 場に出ているカードの枚数
    int rev;            // 革命なら1、そうでないなら0
    int b11;            // 11バックなら1、そうでないなら0（未使用）
    int lock;           // しばりのとき1、そうでないとき0
    int onset;          // 場に何も出ていないとき1、そうでないとき0
    int suit[5];        // 場に出ているカードのマーク。suit[i]が1のとき、
                        // マークがiのカードが出ている。
    int player_qty[5];  // 各プレイヤの残り枚数
    int player_rank[5]; // 各プレイヤのランク
    int seat[5];        // 各席に着いているプレイヤの番号
 
    int joker;          // 自分がJokerを持っているとき1、そうでないとき0。
}*/
