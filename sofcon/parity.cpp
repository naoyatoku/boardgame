#include "board.cpp"    //Moveのため


namespace game
{
    
    static Move parity_solve(const board &b,int max_takes ,int n_all_players )
    {
        int round_size  = max_takes * n_all_players;                   //これは１ラウンドで最大とるコマ数
        int target_take = b.n_empty % round_size;                   //空きコマ数を全員最大数でとった場合にいくつ余るか
        if (target_take >= 1 && target_take <= max_takes) {          //あまりが　1　～　最大とれるマス個数の場合に、それを選択する。（即勝ち）
            //ここで条件に合うようならば、target_take分取れるところをとにかく探す。
            {
                vector<Move>out;
                for(int y=0 ; y <H_SIZ ; ++y){
                    for(int x = 0 ; x < W_SIZ ; ++x){
                        if(b._st[y][x] == EMPTY ){
                            b.enum_all_moves_sub(out,Move(1,Pos(x,y)));
                            if(out.back().n==max_takes){
                                return out.back();
                            }
                        }
                    }
                }
            }
        }
        return Move();  //ない場合には0のMoveを返す。
    }


};