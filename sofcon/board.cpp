#include <windows.h>
#include <random>
#include <chrono>
#include <stdarg.h>

using namespace std;

namespace game
{
    //グリッドサイズをシステム通して同じとします。（この前提で動きます。）
    const   int H_SIZ   = 15;
    const   int W_SIZ   = 15;
    //各種タイル
    const   int WALL   = -1;
    const   int EMPTY  = 0;
    //1-は各プレイヤー陣地です。

    struct Pos {        //グリッド上の位置を表します。こちらの使いやすいようにグリッドサイズを固定します。
        int y;
        int x;
        Pos() : x(0),y(0){}
        Pos(int _x,int _y) : x(_x) , y(_y){}
        Pos(int idx){
            y = idx/H_SIZ;
            x = idx%W_SIZ;
        }
        //配列のインデックスを返します。
        int idx()const{
            return (y*W_SIZ) + x;
        }
        //コピーコンストラクタのために実装します。
        Pos operator=(const Pos&p){
            x = p.x;
            y = p.y;
            return *this;
        }
        //ベクトル足し算です。
        Pos operator+(const Pos&p) const{
            return Pos(x + p.x , y+p.y);
        }
        //同じかどうか
        bool operator==(const Pos&p)const{
            return (x==p.x) && (y==p.y);
        }
    };
    struct Move {
        int cells[3];
        int n;
        Move(int n ,...){
            if(n>3)n=3;
            va_list ap;  va_start(ap,n);
            for(int i=0 ; i < n ; ++i){
                const Pos p = va_arg(ap,Pos);
                cells[i]= p.idx();
            }
            for(int i=n  ; i < 3 ; ++i) { cells[i]=0; }         //残りを0保証します。
            sort();
        }
        void sort(){
            if(n==3){
                if (cells[1] > cells[2])    swap( cells[1] , cells[2] );
                if (cells[0] > cells[1])    swap( cells[0] , cells[1] );
                if (cells[1] > cells[2])    swap( cells[1] , cells[2] );
            }else if(n==2){
                if (cells[0] > cells[1])    swap( cells[0] , cells[1] );                
            }
        }
        //for copy constructor
        Move operator=(const Move &m){
            for(int i=0 ; i < 3 ; ++i){cells[i]=m.cells[i];}
            n = m.n;
            return *this;
        }
    };

    //隣接4方向です。
    static const Pos _dX[4] =  { Pos(-1,0),Pos(1,0),Pos(0,-1),Pos(0,1) };
    static mt19937 rng((uint32_t)chrono::steady_clock::now().time_since_epoch().count());

    //assertは作っておきます。
   
    void _assert( bool a , const char*fmt , ...){
        if(!a){
            va_list ap; va_start(ap,fmt);
            vprintf(fmt , ap);
            exit(1);
        }
    }
};
//
namespace game
{

// ------------------------------------------------------------------ //
//  Boardclass      ※常にMAXN×MAXNをとるようにします。
// ------------------------------------------------------------------ //
class board 
{
protected:
public:
    const   int n_all_player;     //縦横幅と全プレイヤー数
    char    _st[H_SIZ][W_SIZ];    //ステージ
    int     cur_player;           // 1-indexed
    int     n_empty;              //
    int     winner;               //                                

    //=======================================================================================================
    //      constuctor
    //=======================================================================================================
    board(const char st[][W_SIZ], int np, int cur) : n_all_player(np) , cur_player(cur),winner(-1)  //※0でもいいがわかりやすく
    {
        n_empty = copy_grid( _st , st );        //ステージのコピーと空き領域の
    }
    //現在の盤面から、mvを打った後の盤面を作ります。
    board(const board &bd , const Move &mv) : n_all_player(bd.n_all_player),winner(bd.winner)
    {
        n_empty = copy_grid(_st , bd._st);
        apply(mv);        //この状態でapply()します。
    }
    //=======================================================================================================
    //  ここで現在の盤面に、新しい手を打つ動作です。
    //  cur_playerは、打つ前の盤面での手番のプレイヤーです。
    //  applyすると、
    //=======================================================================================================
    void apply(const Move &mv) {
        for (int i = 0; i < mv.n; ++i) {
            Pos p(mv.cells[i]);
            _st[p.y][p.x] = cur_player;     //プレイヤー番号を書く。
        }
        //空きを減らして、もし全部消せたら、終了です。
        if ( (n_empty -= mv.n) == 0) {  winner = cur_player;    }       //ここで勝者が確定。
        else {  //もし終了でなければ、プレイヤーを進める。
            cur_player = (cur_player % n_all_player) + 1;
        }
    }
    //とれる手をすべて列挙します。
    void    enum_all_moves(vector<Move>& mv)const;
    //レンジを外れているか壁の場合
    bool    in_range(const Pos &p)const     {   if (p.y < 0 || p.y >= H_SIZ || p.x < 0 || p.x >= W_SIZ) {return false;}        return true;    }
    //Posを指定して値をとる
    int     val(const Pos& p)const          {   return _st[p.y][p.x];                       }
    //空き(取得可能)かどうか
    bool    avail_to_get(const Pos& p)const {   return in_range(p)==true && val(p)==EMPTY;  }

    //ステージのグリッドをコピーする。
    int copy_grid( char dst[H_SIZ][W_SIZ] , const char src[H_SIZ][W_SIZ] )const
    {
        int empty;
        for (int r = 0; r < H_SIZ ; ++r) {
            for (int c = 0; c < W_SIZ; ++c) {
               dst[r][c] = (char)src[r][c];
                if (dst[r][c] == EMPTY) ++empty;
            }
        }
        return empty;
    }
    //終了しているか
    bool done()const{
        return winner>0;
    }

};



//=======================================================================================================
//      現在のボードでとれる手を全部列挙します。
//=======================================================================================================
void board::enum_all_moves( vector<Move>& moves) const{
    moves.clear();
    for (int y = 0; y < H_SIZ ; ++y ) {                   //    y
        for (int x = 0; x < W_SIZ; ++x ) {                //    x
            if (_st[y][x] != EMPTY) continue;             //    
            Pos _p1(x,y);

            //1マスを取る手をまず登録
            moves.emplace_back(1, _p1);

            //隣接4方向の空きマスを評価します。
            for (int d = 0; d < 4; ++d) {
                Pos _p2( _p1 + _dX[d] );                  //
                if ( avail_to_get(_p2) != true)     continue;   //範囲を外れている。
                if ( _p2.idx() < _p1.idx() )        continue;   //p2が、p1より若い場合、すでに検討済みなので無視します。
                
                //上記以外は2連続として取得できる。
                moves.emplace_back(2 , _p1 , _p2 );

                //3連続を見ていく。選択した二つの、さらに4隣接を見ていきます。
                for (int i = 0; i < 2; ++i) {       //_p1,_p2です。
                    Pos _p3_src(i==0 ? _p1 : _p2);
                    //隣接4方向を見ていきます。
                    for (int d = 0; d < 4; ++d) {
                        Pos _p3( _p3_src + _dX[d] );
                        //
                        //ソースの2個と重複していたり,_p1よりも若いインデックスの場合は重複.
                        if(avail_to_get(_p3)!=true) continue;
                        if( (_p3.idx() < _p1.idx() ) || (_p1==_p3) || (_p3==_p2) )   continue;
                        moves.emplace_back( 3 , _p1 ,_p2 , _p3);
                    }
                }
            }
        }
    }
    //================================================================================
    //  並び変えて、重複を取ります。
    //================================================================================
    std::sort(moves.begin(), moves.end(), [](const Move& a, const Move& b) {    //ソート用ラムダ
        //昇順にならべる
        if (a.n != b.n) return a.n < b.n;                                   //数が少ないほうが手前
        //nが同じならば、全要素を比較します。（n<3でも [n] ～ [2]までの要素は0:同じ値が保証されていて、==になるはずです。
        if (a.cells[0] != b.cells[0]) return a.cells[0] < b.cells[0];
        if (a.cells[1] != b.cells[1]) return a.cells[1] < b.cells[1];
        return a.cells[2] < b.cells[2];
    });
    moves.erase(std::unique(moves.begin(), moves.end(), [](const Move& a, const Move& b) {      //消去用ラムダ。全要素同じならば消します。
        if( a.n != b.n) return false;                                                           //これはn個しか比較しないようします。
        for(int i=0 ; i < a.n  ; ++i){
            if(a.cells[i] != b.cells[i]) return false;
        }
        return true;
    }), moves.end());
}

};      //namespace

/*
// ------------------------------------------------------------------
//      ランダムに打ち手を選択する。
// ------------------------------------------------------------------
static Move fast_random_move(const Board& b) {
    int empties[MAXN * MAXN], ne = 0;
    for (int r = 0; r < b.H; ++r)
        for (int c = 0; c < b.W; ++c)
            if (b.g[r][c] == EMPTY) empties[ne++] = r * b.W + c;


    //とりあえず1つ。ランダムに選択。打ち手を全部列挙してしまうと時間がかかるのですが。
    Move m; m.cells[0] = empties[rng() % ne]; m.n = 1;

    for (int ext = 0; ext < 2; ++ext) {
        if (rng() & 1) break;               //もし奇数ならば？終了

        //
        int src  = m.cells[rng() % m.n];            //どっちかせのセルを選択。

        int r0   = src / b.W, c0 = src % b.W;       
        int dirs[4] = {0, 1, 2, 3};
        for (int i = 3; i > 0; --i) std::swap(dirs[i], dirs[rng() % (i + 1)]);
        bool extended = false;
        for (int di = 0; di < 4 && !extended; ++di) {
            int d  = dirs[di];
            int r2 = r0 + DR[d], c2 = c0 + DC[d];
            if (r2 < 0 || r2 >= b.H || c2 < 0 || c2 >= b.W) continue;
            if (b.g[r2][c2] != EMPTY) continue;
            int nb = r2 * b.W + c2;
            bool dup = false;
            for (int i = 0; i < m.n; ++i) if (m.cells[i] == nb) { dup = true; break; }
            if (!dup) { m.cells[m.n++] = nb; extended = true; }
        }
        if (!extended) break;
    }
    return m;
}
*/