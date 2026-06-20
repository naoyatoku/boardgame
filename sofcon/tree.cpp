#include "board.cpp"        //

namespace game
{
    //-------------------------------------------------------------------------
    //      各ノード情報
    //-------------------------------------------------------------------------
    struct Node {
        board bd;
        Move  mv;
        int   parent;                       //親ノード
        std::vector<int> children;          //子ノード
        int    visit;                       //試行回数
        double value;                       //値
        bool   expanded ;                   //こノードを展開しているか
        //
        Node(const board& b, const Move& m, int _parent): bd(b) ,mv(m),parent(_parent),visit(0),value(0),value(0.0),expanded(false)
        {        
        }
    };

    class tree {
    public:
        std::vector<Node> nodes;
        //---------------------------------------------------------------------------------------
        //      割り付け
        //---------------------------------------------------------------------------------------
        int alloc(const board& b, const Move& m, int parent) {
            nodes.emplace_back(b,m,parent);
            return (int)nodes.size() - 1;
        }
        //---------------------------------------------------------------------------------------
        //      ロールアウト
        //---------------------------------------------------------------------------------------
        // 高速ロールアウト：fast_random_move を使用
        int rollout(board &b) {
            vector<Move>moves;
            b.enum_all_moves(moves);    //すべての打ち手を列挙して、乱数で打ち手を決める。最初オーバーヘッドかかるが、二度目から速い気がします。
            while (!b.done) {
                int idx = rng() % moves.size();     //movesのなかからランダムに取得
                Move m = moves[idx];                //取得します。
                b.apply(m);                         //bに入れます。自動的にプレイヤーは変更される。
                moves.erase( moves.begin() + idx);  //手を削除します。
            }
            return b.winner;
        }
        //------------------------------------------------------------------------------------------------------
        //      子供を展開する  取り得るすべての手を打った盤面を子ノードに追加します。
        //------------------------------------------------------------------------------------------------------
        void expand(int idx) {
            if (nodes[idx].bd.done) { nodes[idx].expanded = true; return; } //  すでに終わっている
            if( nodes[idx].expanded == true ) return;                       //  すでに展開されている。
            const board &src_bd = nodes[idx].bd;                            //  以下、const 参照でアクセスしていきます。
            std::vector<Move> moves;                                        //
            src_bd.enum_all_moves(moves);                                   //  手を列挙します。
            for (const Move& m : moves) {                                   //  すべての列挙した手についてそれぞれノードを追加していきます、
                board new_bd(src_bd);                                       //  
                new_bd.apply(m);                                            //  手を適用します。
                nodes[idx].children.push_back(alloc(new_bd, m, idx));       //  このノードを追加+子供ノードに登録します。
            }
            nodes[idx].expanded = true;
        }
        //------------------------------------------------------------------------------------------------------
        //      子ノードのうち、一番価値が高いものを選択する。
        //      valueが少ないうちは、やったことない手に価値がある（探索優先）
        //      valueが高まってくると、valueの高いものが優先されるようになる。
        //------------------------------------------------------------------------------------------------------
        int select(int idx) {
            while (nodes[idx].expanded &&                   //
                   !nodes[idx].children.empty() &&
                   !nodes[idx].bd.done)
            {
                const Node& n   = nodes[idx];
                double logN     = std::log((double)n.visit + 1.0);
                double best     = -1e18;
                int    best_c   = n.children[0];
                for (int ci : n.children) {
                    const Node& c = nodes[ci];
                    double q = (c.visit > 0) ? (c.value_sum / c.visit) : 0.0;
                    double u = 1.41421356 * std::sqrt(logN / (c.visit + 1.0));
                    if (q + u > best) { best = q + u; best_c = ci; }
                }
                idx = best_c;
            }
            return idx;
        }

        // N人対応バックプロパゲーション：勝者プレイヤーの手番ノードに+1
        void backprop(int idx, int winner) {
            while (idx != -1) {
                nodes[idx].visit++;
                int par = nodes[idx].parent;
                if (par != -1) {
                    if (nodes[par].board.current_player == winner)
                        nodes[idx].value_sum += 1.0;
                }
                idx = par;
            }
        }

    Move best_child(int root) {
        int best = nodes[root].children[0], best_v = -1;
        for (int ci : nodes[root].children)
            if (nodes[ci].visit > best_v) { best_v = nodes[ci].visit; best = ci; }
        return nodes[best].move;
    }

    // ---- 時間ベース探索（本番用） ----
    Move search_timed(const Board& root_board, double budget_ms) {
        auto t0 = std::chrono::steady_clock::now();
        nodes.clear();
        nodes.reserve(4096);

        //--------------------------------------------------------------
        //  現在の盤面をまず登録する。
        //--------------------------------------------------------------
        int root = alloc(root_board, Move{{0,0,0},0}, -1);
        expand(root);
        if (nodes[root].children.empty()) return Move{{0,0,0},0};       //ここで
        //--------------------------------------------------------------
        //  ↑取り得る手にすべての子供を作る。
        //--------------------------------------------------------------
        int sims = 0;
        while (true) {
            // 8シムごとに時間チェック（チェック自体のオーバーヘッドを抑制）
            if ((sims & 7) == 0) {
                double used = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - t0).count();
                if (used >= budget_ms) break;
            }
            //最初から終わっている場合は、
            int leaf = select(root);            //最初の子供です。
            int winner;
            if (nodes[leaf].board.done) {
                winner = nodes[leaf].board.winner;
            } else {
                //終わらない場合は、ここでexpand....
                if (!nodes[leaf].expanded) expand(leaf);
                winner = rollout(nodes[leaf].board);
            }
            backprop(leaf, winner);
            ++sims;
        }
        return best_child(root);
    }

    // ---- sims固定探索（テスト用） ----
    Move search(const Board& root_board, int num_sims) {
        nodes.clear();
        nodes.reserve(num_sims * 4 + 64);
        int root = alloc(root_board, Move{{0,0,0},0}, -1);
        expand(root);
        if (nodes[root].children.empty()) return Move{{0,0,0},0};
        for (int s = 0; s < num_sims; ++s) {
            int leaf = select(root);
            int winner;
            if (nodes[leaf].board.done) {
                winner = nodes[leaf].board.winner;
            } else {
                if (!nodes[leaf].expanded) expand(leaf);
                winner = rollout(nodes[leaf].board);
            }
            backprop(leaf, winner);
        }
        return best_child(root);
    }
};

};