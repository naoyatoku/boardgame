#include "tree.cpp" //board&treeが入っています。


//static const int DR[4] = {-1,  1,  0, 0};
//static const int DC[4] = { 0,  0, -1, 1};
#include <unordered_map>

namespace game
{

    const   int    ENDGAME_MAX_EMPTY = 20;
    const   long   ENDGAME_NODE_CAP  = 8000000;
    const   double ENDGAME_MAX_MS    = 200.0;  // 壁時計上限。超えたらMCTSへフォールバック（失格防止）
// ------------------------------------------------------------------ //
//  終盤厳密解ソルバー（パラノイド完全探索）
//
//  将来の合法手・勝敗は「空きマスの集合」と「手番」だけで決まる
//  （誰が掘ったかは無関係）。空きマスを 0..k-1 に再ラベルしてビット
//  マスクで表し、メモ化付き完全探索で「自分が勝ちを強制できるか」を解く。
//
//  パラノイド仮定：自分以外の全員が自分の勝ちを阻止しようとする。
//   → 「勝てる」と出たら相手が誰でも本当に勝てる（健全）。
//   → 「勝てない」場合は相手のミスで勝てる可能性が残るので MCTS に委ねる。
//
//  n人すべてで正しい（2人なら通常の minimax = Grundy と一致）。
// ------------------------------------------------------------------ //

struct EndgameResult {
    bool solved = false;   // 探索を完了できたか（false=ノード上限超過→MCTSへ）
    bool win    = false;   // 自分が勝ちを強制できるか
    Move move{0};           // win のときに打つべき手
};

class Endgame {
public:
    // empty_count <= max_empty のときだけ呼ぶこと
    EndgameResult solve(const board& b, int me, long node_cap,
                        double max_ms = ENDGAME_MAX_MS) {
        n_ = b.n_all_player;
        me_ = me;
        node_cap_ = node_cap;
        nodes_ = 0;
        start_ = std::chrono::steady_clock::now();
        max_ms_ = max_ms;
        memo_.clear();

        // 空きマスを列挙して 0..k-1 に再ラベル
        cells_.clear();

        //
        int idx_of[H_SIZ * W_SIZ];
        for (int i = 0; i < H_SIZ * W_SIZ ; ++i) idx_of[i] = -1;

        //------------------------------------------------------
        //  盤面を見て、空きますをidx_ofに入れていく。
        //------------------------------------------------------
        for (int r = 0; r < b.H; ++r)
            for (int c = 0; c < b.W; ++c)
                if (b.g[r][c] == EMPTY) {
                    idx_of[r * b.W + c] = (int)cells_.size();
                    cells_.push_back(r * b.W + c);
                }
        //------------------------------------------------------
        //  idx_of[]  ... [空きます] →  cellsのインデックス
        //  cells[]   ... 空きますインデックス
        //------------------------------------------------------
        int k = (int)cells_.size();
        //広さはMAXWとする
        W_ = b.W;

        //
        if (k == 0 || k > 30) return {};   // 32bit マスクの安全域

        //---------------------------------------------------------------------
        //      隣接（4連結）を構築
        //---------------------------------------------------------------------
        adj_.assign(k, {});                                     //空きます分の二次元配列。
        for (int i = 0; i < k; ++i) {                           //すべての空きますについて
              //4隣接候補について
            for (int d = 0; d < 4; ++d) {
                //隣接ポイントです。
                Pos dp( Pos(cells_[i]) + _dX[d] );
                if(b.in_range(dp)!=true)continue;
                //空きます登録されている（要するに空き）
                //  ->  adj_[i]（いま評価している空きます）に、隣接の空きを登録する。登録はcellsのインデックス。
                if (idx_of[ dp.idx() ] >= 0) adj_[i].push_back(idx_of[dp.idx()]);
            }
        }
        //---------------------------------------------------------------------
        //  この時点で、
        //  adj[0番目空きます] : 隣接する空きますのvector<int> : cellsのindex
        //      :
        //  adj_[n番目空きます]
        //---------------------------------------------------------------------
        // 連結する 1?3 マスの手をすべてビットマスクで列挙
        buildMoves(k);          //全パターンを32bit割付のvecotr<int>配列にする

        uint32_t full = (k == 32) ? 0xffffffffu : ((1u << k) - 1u);     //k個分の空きます（空き＝１）
//        int cur_off = 0;   // 根は自分の手番（off=0）

        // 根：自分の手番。勝ちを強制できる手を探す

       
        //----------------------------------------------------------------------------------------------
        //  
        //----------------------------------------------------------------------------------------------

        EndgameResult res;
        for (uint32_t mv : moveMasks_) {                    //全moveです。
            if ((mv & full) != mv) continue;               // mvはbitですので、&とりmvにならない場合は空いてない

            uint32_t nm = full & ~mv;                      //nm  : mvを取り除いた（とった）全空きます
            bool win;
            if (nm == 0) win = true;                       // 最後のマスを取った→自分勝ち
            else {
                int r = rec(nm, 1 % n_);                    //nm に対して、 1%プレイヤー数 (=1)でrecを呼び出す。

                if (r < 0) return {};                       // ノード上限→未解決
                win = (r == 1);
            }
            if (win) { res.solved = true; res.win = true; res.move = decode(mv); return res; }
        }
        res.solved = true; res.win = false;                 // 勝ちを強制できない
        return res;
    }

private:
    int n_, me_, W_;
    long node_cap_, nodes_;
    std::chrono::steady_clock::time_point start_;
    double max_ms_;
    std::vector<int> cells_;
    std::vector<std::vector<int>> adj_;
    std::vector<uint32_t> moveMasks_;
    std::unordered_map<uint64_t, char> memo_;

    //----------------------------------------------------------------------------
    //  bit割付けの空きますマッピング
    //  cellsのインデックスなので、0-kの数値に収まっているので、bit割付可能。
    //----------------------------------------------------------------------------
    void buildMoves(int k)  {
        moveMasks_.clear();
        std::vector<uint32_t> tmp;
        for (int i = 0; i < k; ++i) {
            //--------------------------------
            //  1つのパターン
            //--------------------------------
            tmp.push_back(1u << i);
            //--------------------------------
            //  2つのパターン
            //  隣接4個(adk_[i])を登録します。
            //--------------------------------
            for (int j : adj_[i]) {
                if (j < i) continue;                            //iよりも小さいjはすでに考慮済み（逆で）
                uint32_t _2_cells = (1u << i) | (1u << j);      //隣接の2パターン   //
                tmp.push_back(_2_cells);
                //----------------------------------
                //  ここで3つのパターンも走査できる
                //----------------------------------
                {
                    for (int src : {i, j})                      //３つめのますのどちらをベースにするか
                    for (int e : adj_[src]) {                    //ベースとなるセルの四隣接
                        if (e == i || e == j) continue;
                        tmp.push_back(_2_cells | (1u << e));        //これがベースとなるセルの四離接から選んだ3つめのあきセル。
                    }
                }
            }
        }
        // 重複除去
        std::sort(tmp.begin(), tmp.end());
        tmp.erase(std::unique(tmp.begin(), tmp.end()), tmp.end());
        moveMasks_.swap(tmp);       //これがoutput
    }

    Move decode(uint32_t mv) {
        Move m; m.n = 0;
        for (int i = 0; i < (int)cells_.size() && m.n < 3; ++i)
            if (mv & (1u << i)) m.cells[m.n++] = cells_[i];
        return m;
    }

    // mask の状態・手番 off で「自分(off=0基準のme)が勝ちを強制できるか」
    //   返り値: 1=勝てる, 0=勝てない, -1=ノード上限で未解決
    int rec(uint32_t mask, int off) {
        if (++nodes_ > node_cap_) return -1;                        //再起呼び出しによってノード数上限を

        //---------------------------------------------------------------------------
        //  時間チェック
        //---------------------------------------------------------------------------
        if ((nodes_ & 0xffff) == 0) {                                       //0x10000回(6万回くらい？)に一度時間のチェック
            double el = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start_).count();
            if (el > max_ms_) return -1;
        }

        //---------------------------------------------------------------------
        //  このkeyは、　　盤面 << 6  | プレイヤー番号(6bit): memo_への
        //---------------------------------------------------------------------
        uint64_t key = ((uint64_t)mask << 6) | (uint32_t)off;
        auto it = memo_.find(key);
        if (it != memo_.end()) return it->second;                   //すでにmemoに残っていればvalueを返す。

        //---------------------------------------------------------------------
        //  ここではプレイヤーは0-オリジンでです。
        //---------------------------------------------------------------------
        bool my_turn = (off == 0);
        // my_turn: 1手でも勝てれば勝ち / 相手手番: 全手で勝てないと勝ちにならない
        int result = my_turn ? 0 : 1;
        bool decided = false;

        //---------------------------------------------------------------------
        //  すべての動作に対して、
        //---------------------------------------------------------------------
        for (uint32_t mv : moveMasks_) {
            if ((mv & mask) != mv) continue;                 // 適用不可
            uint32_t nm = mask & ~mv;                       //mvで盤面を取ります。
            int child;
            if (nm == 0) {                                      //とった結果、全部埋まった。
                //一番深いところで、ここに来ます。
                // この手番のプレイヤーが最後のマスを取った → そのプレイヤーの勝ち
                //childは、my_turnのときに 1 : 0
                child = my_turn ? 1 : 0;
            } else {
                int r = rec(nm, (off + 1) % n_);            //次の人の手番で、recを呼び出します。
                                                            //いったん、一番深いところまで行く。
                if (r < 0) return -1;                        // 上限伝播
                child = r;
            }
            //ここで最善手を判断する。
            if (my_turn) {
                if (child == 1) { result = 1; decided = true; break; }  // 勝ち手発見
            } else {
                if (child == 0) { result = 0; decided = true; break; }//
            }
        }
        (void)decided;
        memo_[key] = (char)result;
        return result;
    }
};

};      //namespace  game