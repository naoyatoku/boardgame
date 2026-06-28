#pragma once
#include <cmath>
#include <cstring>
#include <algorithm>
#include <vector>
#include <cstdio>
#include <cstdlib>

#include "board.cpp"

using namespace game;


#if 1       //速度のため、本番で消えるアサート
#define ASSERT(cond, ...) \
    do { \
        if (!(cond)) { \
            std::fprintf(stderr, "Assert failed: %s (%s:%d)\n  ", \
                         #cond, __FILE__, __LINE__); \
            std::fprintf(stderr, __VA_ARGS__); \
            std::fprintf(stderr, "\n"); \
            std::abort(); \
        } \
    } while (0)
#else
#define ASSERT(cond, ...) ((void)0)
#endif


namespace nn {
//vector等は使わないで、配列でnewだけにしておきます。もしくはポインタだけ指示して
class tensor
{
private:
    const float   * __p_arr;      //toku　これは、tensor_2dが並んでいるとみなします。
    int             __alt;        //先頭アドレスを変化させられるためのオフセット値です。（位置ずらすときに、いちいち新しいオブジェクトを作らなくていいように）
    int             __size;
protected:
    //各テンソルのサイズ分のオフセットをさせる。(戻り値はその時の左上アドレス)
    void     _alt(int n){
        __alt = n * __size;
    }

public:
    tensor(const float* p_arr, int sz) : __p_arr(p_arr), __alt(0), __size(sz) {}
    //基準ポイントをそれぞれ計算したい場所に応じて変化させられるように
    //各テンソルの左上アドレスを返す
    const float *p_head()const{
        return __p_arr + __alt;
    }
    int size()const { return __size; }



};
class tensor_2d : public tensor
{
    //shape
    const int __W;
    const int __H;
protected:
public:
    tensor_2d(const float* p_arr, int h, int w) : tensor(p_arr, h* w), __H(h), __W(w) {}

    //この二次元テンソルが並んでいるとして、n面目の先頭アドレスを返すようにします。
    //  float *p_head_n(int n)const {   return  __p_arr + (n*size());   }    //とりあえず、
        //※convのpadding対応のために、ofsをもった場所を指示できるようにします。
    float dot(const tensor_2d& a, int x_ofs = 0, int y_ofs = 0)const {
        float ret = 0.0;
        //dot積の範囲は、aに合わせます。
        for (int y = 0; y < a.H(); ++y) {
            for (int x = 0; x < a.W(); ++x) {                   //aの範囲を全部、
                ret += v(x + x_ofs, y + y_ofs) * a.v(x, y);             //自分の範囲は、ofs分ずらした位置
            }
        }
        return ret;
    }
    //先頭位置をずらす
    tensor_2d& alt(int n) { tensor::_alt(n);        return *this; }

    //このhsapeの配列へアクセスするような値を返す関数。[][]のような
    float v(int x, int y)const {        //p_headは好きな場所を指示する場合に使います。
        if (x < 0 || x >= __W || y < 0 || y >= __H) {
            return 0;                   //範囲外は0とします。
        }
        return *(p_head() + (y * __W * x));
        //                  ^^^^^^^^^^^
        //                  この平面がどこのオフセットかを自由に設定できる。（保証は呼び出し側で）
    }
    int H()const { return __H; }
    int W()const { return __W; }
};
//配列実体は内包しないで、アクセスヘルパーです。
class tensor_3d :public tensor
{
    const int   __D;          //これが奥行。
    const int   __H;
    const int   __W;          //
    //高速化のため、2dテンソルのウィンドウキャッシュです。
    tensor_2d   __win_cache;        //
protected:
public:    
    //3dテンソルが並んでいるとして、n番目の先頭アドレスを返す。
//  float *p_headn_n(int n){      }
    //この3dテンソルは、
    //      A():    9×15*15
    //      K():    9*3*3
    //paddingの処理も必要。
    tensor_3d(float *p_arr , int d , int h , int w) : tensor(p_arr,d*h*w),__D(d),__H(h),__W(w),__win_cache(p_arr,h,w) {}

    //aの深さに合わせて、dotをとります。
    float dot( tensor_3d& a,int x_ofs=0,int y_ofs=0){
        float ret=0.0;
        for(int z = 0 ; z < a.D() ; ++z ){      //このZレイヤーの面のdot積を考える。aは左上から、スタート、自分自身は、どこの部分を
            //このテンソルのzの先頭
            ret += win(z).dot(a.win(z), x_ofs, y_ofs);
        }
        return ret;
    }
    //先頭位置をずらす
    tensor_3d &alt(int n){        tensor::_alt(n);  return *this;   }
    //このテンソルが並んでいるとしてidx番目の3dテンソルなかの値を取り出す。
    float v(int x,int y ,int z)const{
        //範囲外はここで
        if(z < 0 || z >= __D ){
            return 0.0;
        }
        tensor_2d window(p_head(), __H, __W);
        return window.alt(z).v(x,y);
    }
    tensor_2d& win(int z) {     //z指示したときの2dテンソルウィンドウを返す。(not thread safe)
        ASSERT(z >= 0 && z < __D, "tensor_3d::win : overflow");
        return __win_cache.alt(z);
    }
    int D()const { return __D; }
    int H()const { return __H; }
    int W()const { return __W; }
};


//---------------------------------------------------------------------
//  最初の盤面を、ブロック種類によって、9種類の盤面に分解します。
//  [0]   :   壁だけ集めた
//  [1]   :   空きだけ集めた
//  [2]   :   必ず自分
//  [3]   :   他プレイヤー 1
//  [4]   :   他プレイヤー 2
//  [5]   :   他プレイヤー 3
//  [6]   :   他プレイヤー 4
//  [7]   :   これは空き（n_all_playrが6になってもできるように）→5人対戦のときは0
//  [8]   :   プレイヤー人数などの文脈を教えるためのデータする。
//----------------------------------------------------------------------
//         static float nn_input[9 * MAXN * MAXN];         //9画面分    →   outとして引数に入ります。
    static void board_to_nn_input(const board& b, float* out /*must 9×15×15 */) {
        const int hw = game::H_SIZ * game::W_SIZ;               //1盤面サイズ
        std::fill(out, out + (9 * hw), 0.f);                    //0でfillします。 9×盤面サイズ分
        for (int y = 0; y < game::H_SIZ ; ++y) {
            for (int x = 0; x < game::W_SIZ ; ++x) {            //
//                int idx = r * b.W + c;
                game::Pos p(x,y);
                int8_t cell = b._st[y][x];
                if (cell == WALL)  out[p.idx] = 1.f;     //壁は[0]
                if (cell == EMPTY) out[hw + p.idx] = 1.f; //空きは[1]
                //各プレイヤーの盤面を登録。cur_player基準で[2]-[7]まで入れていく。（）
                for (int k = 0; k < b.n_all_player && k < 6; ++k) {             //kは盤面のインデックスです。0がcur_playerとなるように
                    int pid = (b.cur_player - 1 + k) % b.n_all_player + 1;      //cur_playerが pid ==1 になるように,全プレイヤーの番号を作る。k=0,..,5です
                    if (cell == (int8_t)pid) out[(2 + k) * hw + p.idx] = 1.f;   //該当する盤面データ[k]1を書きます。
                }
            }
        }
        //盤面[8]は、プレイ人数などの情報を入れる。
        float np_norm = (float)(b.n_all_player - 1) / 5.f;     //プレイ人数を最大6人ならば  (0～5) / 5　で、 0.0 - 5.0までの間の値として、
        std::fill(out + 8 * hw, out + 9 * hw, np_norm);        //  盤面一面に置きます。（これは一つの特徴）
    }

};
// namespace sofcon_nn