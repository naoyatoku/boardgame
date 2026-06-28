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

    //列挙する打ち手の制限です。
    #define DEFAULT_ENUM_LIMIT  256

    void _assert(bool a, const char* fmt, ...) {
        if (!a) {
            va_list ap; va_start(ap, fmt);
            vprintf(fmt, ap);
            exit(1);
        }
    }

    //使いやすいようにインデックスと座標の両方を持たせてみます。
    struct Pos {        //グリッド上の位置を表します。こちらの使いやすいようにグリッドサイズを固定します。
        int y;
        int x;
        int idx;

        Pos() : x(0),y(0){}
        Pos(int _x,int _y) : x(_x) , y(_y)  {     idx = (y * W_SIZ) + x;            }
        Pos(int _idx) : idx(_idx)           {      y = idx/W_SIZ; x = idx%W_SIZ;    }
        //配列のインデックスを返します。
/*        int idx()const{
            return (y*W_SIZ) + x;
        }
            */
        //コピーコンストラクタのために実装します。これいらないんですかね
        Pos operator=(const Pos&p){
            x = p.x;
            y = p.y;
            idx = p.idx;
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
        bool operator!=(const Pos& p)const {
            return ! operator==(p);
        }

        bool operator>(const Pos&p)const{
            return idx > p.idx;
        }
        bool operator<(const Pos&p)const{
            return idx < p.idx;
        }

    };
    struct Move {
        Pos cells[10];
        int n;
        Move(int _n ,...) : n(0) {
            va_list ap;  va_start(ap,_n);
            for(int i=0 ; i < _n ; ++i){
                cells[i]=va_arg(ap,Pos);
            }
            std::sort(cells , cells+n);
        }
        //default
        Move(): n(0){}
        //for copy constructor
        Move operator=(const Move &m){
            for(int i=0 ; i < 3 ; ++i){cells[i]=m.cells[i];}
            n = m.n;
            return *this;
        }
        //含まれているかチェックします。
        bool include(const Pos&c)const{
            for(int i=0 ; i < n ; ++i){
                if(cells[i].idx == c.idx)return true;
            }
            return false;
        }
        //新たにPosを足す。
        Move add(const Pos&new_c)
        {
            _assert( (n+1) < 10 , "Move::add() overflow");
            cells[n] = new_c;
            n++;
            return *this;
        }

    };

    //隣接4方向です。
    static const Pos _dX[4] =  { Pos(-1,0),Pos(1,0),Pos(0,-1),Pos(0,1) };
    static mt19937 rng((uint32_t)chrono::steady_clock::now().time_since_epoch().count());

    //assertは作っておきます。
   

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
    const   int n_all_player;       //縦横幅と全プレイヤー数
    const   int n_max_takes;        //1回にとれるセル数
    char    _st[H_SIZ][W_SIZ];      //ステージ
    int     cur_player;             // 1-indexed
    int     n_empty;                //
    int     winner;                 //                                

    //=======================================================================================================
    //      constuctor
    //=======================================================================================================
    board(const char st[][W_SIZ], int np, int cur,int max_takes) : n_all_player(np) , cur_player(cur),n_max_takes(max_takes) , winner(-1)  //※0でもいいがわかりやすく
    {
        n_empty = copy_grid( _st , st );        //ステージのコピーと空き領域の
    }
    //現在の盤面から、mvを打った後の盤面を作ります。
    board(const board &src , const Move &mv) : n_all_player(src.n_all_player),n_max_takes(src.n_max_takes) , winner(src.winner)
    {
        n_empty = copy_grid(_st , src._st);
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
    void    enum_all_moves_sub(vector<Move>& mv,Move seed,int limit = DEFAULT_ENUM_LIMIT)const;

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
void    board::enum_all_moves_sub(vector<Move>& out,Move seed,int limit )const {
    const int anchor = seed.cells[0].idx;
    //まず最初のseedについて、outにたします。
    out.push_back(seed);        //n=1のリストとなります。
    int start_idx = out.size()-1;              //startが、outの中の
    for( int n= 1 ; n < n_max_takes ; ++n)    {     //個数分のセルを作ります。
        //------------------------------------------------------------------------------------------------------
        //  n個のセルを持つoutはstart_idxから始まり、このループの最後にn+1個のMoveが積み重なっていきます。
        //  もし前段でセルが足されずに、n個のセルがない場合は、
        //  start_idxがout[start_idx]が配列の外になりますのでこのループに入りません。
        //------------------------------------------------------------------------------------------------------
        for(int i=start_idx ; (i < out.size()) && (out[i].n==n) ; ++i , ++start_idx ){      //out[i]の各セルの隣接セルで空いているものを足していきます。
            for(int j=0 ; j < out[i].n ; ++j ){                       //out[i]の持っているセルの個数
                for(int d = 0 ; d < 4 ; ++d){                         //  4近傍
                    Pos cell = out[i].cells[j] + _dX[d];               //  4隣接の一つを作る。
                    if(in_range(cell)!=true )         continue;       //  ステージ範囲内
                    if(_st[cell.y][cell.x]!=EMPTY)    continue;       //  空きのみ
                    if(cell.idx < anchor)             continue;       //  前のものは考慮済
                    if(out[i].include(cell))          continue;       //  重複しているのは除外
                    //OKなので、out[i]に新しいセルをくっつけたものを足していきます。(n+1)個のセルになるはずです。
                    {
                        Move    new_mv = out[i];
                        out.push_back(new_mv.add(cell));
                        if(out.size()==limit)goto _fin;     //個数制限に来たら強制終了です。
                    }
                }
            }
        }
    }
_fin:;
}
//void    enum_all_moves(vector<Move>& mv)const;
void board::enum_all_moves(std::vector<Move>& out)const
{
    out.clear();
    for (int y = 0; y < H_SIZ ; ++y)
        for (int x = 0; x < W_SIZ; ++x) {
            if (_st[y][x] != EMPTY) continue;
//            Move m{}; m.n=1; m.cells[0]=r*b.W+c;
            enum_all_moves_sub( out , Move(1,Pos(x,y)) );
        }
    // 各手のセルをソートして正準化
    for (Move& m : out)
        std::sort(m.cells, m.cells+m.n);
    // 重複除去
    std::sort(out.begin(), out.end(), [](const Move& a, const Move& b){
        if (a.n!=b.n) return a.n<b.n;
        for (int i=0;i<a.n;i++) if (a.cells[i]!=b.cells[i]) return a.cells[i]<b.cells[i];
        return false;
    });
    out.erase(std::unique(out.begin(), out.end(), [](const Move& a, const Move& b){
        if (a.n!=b.n) return false;
        for (int i=0;i<a.n;i++) if (a.cells[i]!=b.cells[i]) return false;
        return true;
    }), out.end());
}

#if 0
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

    //max_take



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
#endif

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