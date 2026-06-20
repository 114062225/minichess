#include <utility>
#include "state.hpp"
#include "quiescence.hpp"
#include "minimax.hpp"

/*============================================================
 * eval_ctx
 *============================================================*/
int Quiescence::eval_ctx(
    State *state,
    int depth,
    int alpha,
    int beta,
    GameHistory& history,
    int ply,
    SearchContext& ctx,
    const MMParams& p
){
    ctx.nodes++;
    if(ply > ctx.seldepth){
        ctx.seldepth = ply;
    }
    if(ctx.stop){
        return 0;
    }

    /* === Lazy move generation (sets game_state) === */
    if(state->legal_actions.empty() && state->game_state == UNKNOWN){
        state->get_legal_actions();
    }

    /* === Terminal / leaf checks === */
    if(state->game_state == WIN){
        return P_MAX - ply;
    }

    if(state->game_state == DRAW){
        return 0;
    }

    /* === Repetition check (game-specific) === */
    int rep_score;
    if(state->check_repetition(history, rep_score)){
        return rep_score;
    }

    history.push(state->hash());

    if(depth <= 0){
        int score = Quiescence::search_qs(
            state,
            alpha,
            beta,
            history,
            ctx
        );
        history.pop(state->hash());
        return score;
    }

    /* === Negamax loop === */
    int best_score = M_MAX;

    // 加入 first flag
    bool first = true;

    auto is_capture = [&](const Move& m){
        auto [from, to] = m;
        return state->piece_at(1 - state->player, to.first, to.second) != 0;
    };

    std::sort(state->legal_actions.begin(), state->legal_actions.end(),
        [&](const Move& a, const Move& b){
            return is_capture(a) > is_capture(b);
        }
    );


    for(auto& action : state->legal_actions){
        State* next = state->next_state(action);

        bool same = next->same_player_as_parent();

        int raw, score;
        // 第一個 move 正常
        if(first){
            raw = eval_ctx(
                next,
                depth - 1,
                -beta,
                -alpha,
                history,
                ply + 1,
                ctx,
                p
            );
            if(same) score = raw;
            else score = -raw;
            first = false;
        }
        else{
            // 先做「null window search」
            raw = eval_ctx(
                next,
                depth - 1,
                -alpha - 1,   // ⭐ 關鍵
                -alpha,
                history,
                ply + 1,
                ctx,
                p
            );
            if(same) score = raw;
            else score = -raw;

            // 可能變好才重算
            if(score > alpha){
                raw = eval_ctx(
                    next,
                    depth - 1,
                    -beta,
                    -alpha,
                    history,
                    ply + 1,
                    ctx,
                    p
                );
                if(same) score = raw;
                else score = -raw;
            }
        }

        delete next;

        if(score > best_score){
            best_score = score;
        }

        if(score > alpha){
            alpha = score;
        }

        if(alpha >= beta){
            break;
        }
    }

    history.pop(state->hash());
    return best_score;
}

/*============================================================
 * search
 *============================================================*/
SearchResult Quiescence::search(
    State *state,
    int depth,
    GameHistory& history,
    SearchContext& ctx
){
    ctx.reset();
    MMParams p = MMParams::from_map(ctx.params);
    SearchResult result;
    result.depth = depth;

    if(!state->legal_actions.size()){
        state->get_legal_actions();
    }

    int best_score = M_MAX - 10;
    int move_index = 0;
    int total_moves = (int)state->legal_actions.size();

    for(auto& action : state->legal_actions){
        State* next = state->next_state(action);

        bool same = next->same_player_as_parent();

        int raw = eval_ctx(
            next,
            depth - 1,
            M_MAX,
            P_MAX,
            history,
            1,
            ctx,
            p
        );

        int score;
        if(same) score = raw;
        else score = -raw;

        delete next;

        if(score > best_score){
            best_score = score;
            result.best_move = action;

            if(p.report_partial && ctx.on_root_update){
               ctx.on_root_update({result.best_move, best_score, depth, move_index + 1, total_moves});
            }
        }
        move_index++;
    }

    result.score = best_score;
    return result;
}

int Quiescence::search_qs(
    State* state,
    int alpha,
    int beta,
    GameHistory& history,
    SearchContext& ctx
)
{
    int stand = state->evaluate(true, true, &history);

    if(stand >= beta) return stand;
    if(stand > alpha) alpha = stand;

    if(state->legal_actions.empty() && state->game_state == UNKNOWN){
        state->get_legal_actions();
    }

    for(auto& action : state->legal_actions){
        auto [from, to] = action;

        // ✅ 只處理吃子
        if(state->piece_at(1 - state->player, to.first, to.second) == 0)
            continue;

        State* next = state->next_state(action);

        bool same = next->same_player_as_parent();

        int raw = search_qs(next, -beta, -alpha, history, ctx);
        int score = same ? raw : -raw;

        delete next;

        if(score >= beta) return score;
        if(score > alpha) alpha = score;
    }

    return alpha;
}

/*============================================================
 * default_params / param_defs
 *============================================================*/
ParamMap Quiescence::default_params(){
    return {
        {"UseKPEval", "true"},
        {"UseEvalMobility", "true"},
        {"ReportPartial", "true"},
    };
}

std::vector<ParamDef> Quiescence::param_defs(){
    return {
        {"UseKPEval", ParamDef::CHECK, "true"},
        {"UseEvalMobility", ParamDef::CHECK, "true"},
        {"ReportPartial", ParamDef::CHECK, "true"},
    };
}
