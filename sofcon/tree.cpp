#include "board.cpp"        //

namespace game
{
    //-------------------------------------------------------------------------
    //      各ノード情報
    //-------------------------------------------------------------------------
    struct Node {
        //====================================================
        //  これは親ノードの情報です。
        //  この盤面になったコンテキストという意味合いです
        //====================================================
        Move  parent_moved;        //これは、どの手を親が打った結果の盤面なのかを表します。
        int   parent;              //親ノード

        //====================================================
        //  ここからは、現在の盤面の情報です。
        //====================================================
        board bd;                           //現在の盤面です。
        std::vector<int> children;          //子ノード
        int    visit;                       //試行回数
        double value;                       //値
        //
        Node(const board& _parent_bd, const Move& _parent_mv, int _parent): bd(_parent_bd,_parent_mv) ,parent_moved(_parent_mv),parent(_parent),visit(0),value(0.0)
        {

        }
        //子供が展開されているかどうかを解答する。
        bool expanded()const{
            if(bd.done() == true)   return true;
            //盤面が終わっていない場合は、子供が必ずあるはずなのでそれを解答します。
            return children.size()>0;
        }
    };

    class tree {
    public:
        std::vector<Node> nodes;
        //---------------------------------------------------------------------------------------
        //      割り付け
        //---------------------------------------------------------------------------------------
        int add_node(const board& src_bd, const Move& src_mv, int parent) {
            nodes.emplace_back(src_bd,src_mv,parent);
            return (int)nodes.size() - 1;
        }
        //---------------------------------------------------------------------------------------
        //      ロールアウト
        //---------------------------------------------------------------------------------------
        // 高速ロールアウト：fast_random_move を使用
        int rollout(board &b) {
            vector<Move>moves;
            b.enum_all_moves(moves);    //すべての打ち手を列挙して、乱数で打ち手を決める。最初オーバーヘッドかかるが、二度目から速い気がします。
            while (!b.done()) {
                int idx = rng() % moves.size();     //movesのなかからランダムに取得
                Move mv = moves[idx];                //取得します。
                b.apply(mv);                         //bに入れます。自動的にプレイヤーは変更される。
                //
                {
                    std::vector<Move> next_moves;
                    for (const Move& m : moves) {
                        bool contains = false;
                        for (int i = 0; i < mv.n && !contains; ++i)
                            for (int j = 0; j < m.n && !contains; ++j)
                                if (m.cells[j] == mv.cells[i]) contains = true;
                            if (!contains) next_moves.push_back(m);
                    }
                    moves = std::move(next_moves);
                }

                //moves.erase( moves.begin() + idx);  //手を削除します。
            }
            return b.winner;
        }
        //------------------------------------------------------------------------------------------------------
        //      子供を展開する  取り得るすべての手を打った盤面を子ノードに追加します。
        //------------------------------------------------------------------------------------------------------
        void expand(int idx) {
            if (nodes[idx].bd.done()) { return; } //  すでに終わっている
            if( nodes[idx].expanded() == true ) return;                     //  すでに展開されている。
            std::vector<Move> moves;                                        //
            nodes[idx].bd.enum_all_moves(moves);                            //  手を列挙します。
            for (const Move& mv : moves) {                                   //  すべての列挙した手についてそれぞれノードを追加していきます、
                nodes[idx].children.push_back(add_node(nodes[idx].bd  , mv , idx));       //  このノードを追加+子供ノードに登録します。(allocでnoteに追加、push_back登録されるのはindx.)
            }
        }
        //------------------------------------------------------------------------------------------------------
        //      子ノードのうち、一番価値が高いものを選択する。
        //      valueが少ないうちは、やったことない手に価値がある（探索優先）
        //      valueが高まってくると、valueの高いものが優先されるようになる。
        //      もし選択するべき子供の候補ない場合は、assertするか、-1を返します。
        //------------------------------------------------------------------------------------------------------
        int select(const int idx,bool assert_if_no_children=true) {
            //ここでは、ndoes[idx]の子供のうち、今回選択すべきものを選ぶ関数です。
            //while(claudeが書いたもの)は変だ。

            //expandしてなければします。
            if( nodes[idx].expanded != true){
                expand(idx);
            }
            //こどもがいない場合には異常を返します。
            if(nodes[idx].children.empty()){
                if(assert_if_no_children)   _assert( 0 , "tree:select() no children");
                return -1;  //
            }
            //選択するべきidx
            int best_idx;
            {
                const Node& me   = nodes[idx];
                double logN      = std::log((double)me.visit + 1.0);      //試行回数のlnをとることで指数の値にする。
                //初期設定
                double best     = -1e18;                                //価値の最大
                int    best_c   = me.children[0];                        //最大の価値を持つ子供のidx

                for (int c_idx : me.children) {
                    const Node& c = nodes[c_idx];
                    double q = (c.visit > 0) ? (c.value / c.visit) : 0.0;           //q :   勝利の割合（勝率）
                    double u = 1.41421356 * std::sqrt(logN / (c.visit + 1.0));      //u :   √2 * √ ln(親の試行回数)/子の試行回数
                    if (q + u > best) { best = q + u; best_c = c_idx; }             //q+uを価値として最大を見つけます。
                }
                best_idx = best_c;
            }
            return best_idx;
        }
        //------------------------------------------------------------------------
        //  idxでのwinnwerを、親ノードに伝播させるが、その際、その盤面での手番
        //  プレイヤーとwinnerが一致していればOKとする。
        //  idxは、一つ上の
        //------------------------------------------------------------------------
        void back_propagation(int idx, int winner) {
            while (idx != -1) {                         //これはrootのみです。
                nodes[idx].visit++;                     //idxノードの試行回数は +1
                int par = nodes[idx].parent;            //親ノードです。
                if (par != -1) {
                    if (nodes[par].bd.cur_player == winner) //これがわかりにくい。
                        nodes[idx].value += 1.0;
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
    Move search_timed(const board& root_board, double budget_ms) {
        auto t0 = std::chrono::steady_clock::now();
        nodes.clear();
        nodes.reserve(4096);

        //--------------------------------------------------------------
        //  現在の盤面をまず登録する。
        //--------------------------------------------------------------
        int root =add_node(root_board, Move{{0,0,0},0}, -1);
        expand(root);
//      if (nodes[root].children.empty()) return Move{{0,0,0},0};           //ここで手がない。のは、おかしい。
        _assert( ! nodes[root].children.empty() , "board has alrady done");
        //--------------------------------------------------------------
        //  rootは展開されています。
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
            //=============================================================
            //  rootの子供のうちベストな手を選択します。
            //=============================================================
            int leaf = select(root);                        //leafが選択された子供      //rootが選択するプレーヤの手番で、それが打った手がleaf.
            int winner;
            //
            if (nodes[leaf].bd.done()) {                   //ここで終わった場合はwinnerが決定できる
                winner = nodes[leaf].bd.winner;          //leaf
            } else {
                //終わらない場合は、ここでexpand....
//                if (!nodes[leaf].expanded) expand(leaf);
                winner = rollout(nodes[leaf].bd);       //この盤面で
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