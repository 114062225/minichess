#include <utility>
#include "state.hpp"
#include "alphabeta.hpp"
#include "minimax.hpp"   // 用 MMParams

/*============================================================
 * AlphaBeta — eval_ctx (從 minimax 改)
 *============================================================*/
int AlphaBeta::eval_ctx(
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

    /* lazy move gen */
    if(state->legal_actions.empty() && state->game_state == UNKNOWN){
        state->get_legal_actions();
    }

    /* terminal */
    if(state->game_state == WIN){
        return P_MAX - ply;
    }

    if(state->game_state == DRAW){
        return 0;
    }

    int rep_score;
    if(state->check_repetition(history, rep_score)){
        return rep_score;
    }

    history.push(state->hash());

    if(depth <= 0){
        int score = state->evaluate(
            p.use_kp_eval, p.use_eval_mobility, &history
        );
        history.pop(state->hash());
        return score;
    }

    int best = M_MAX;

    for(auto& action : state->legal_actions){
        State* next = state->next_state(action);

        int score = -eval_ctx(
            next,
            depth - 1,
            -beta,
            -alpha,
            history,
            ply + 1,
            ctx,
            p
        );

        delete next;

        /* 和 minimax 一樣 */
        if(score > best){
            best = score;
        }

        /* ✅ Alpha update */
        if(score > alpha){
            alpha = score;
        }

        /* ✅ pruning（唯一新東西）*/
        if(alpha >= beta){
            break;
        }
    }

    history.pop(state->hash());
    return best;
}

SearchResult AlphaBeta::search(
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

    int best_score = M_MAX;

    for(auto& action : state->legal_actions){
        State* next = state->next_state(action);

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

        int score = -raw;

        delete next;

        if(score > best_score){
            best_score = score;
            result.best_move = action;
        }
    }

    result.score = best_score;
    return result;
}

ParamMap AlphaBeta::default_params(){
    return {
        {"UseKPEval", "true"},
        {"UseEvalMobility", "true"},
        {"ReportPartial", "true"},
    };
}

std::vector<ParamDef> AlphaBeta::param_defs(){
    return {
        {"UseKPEval", ParamDef::CHECK, "true"},
        {"UseEvalMobility", ParamDef::CHECK, "true"},
        {"ReportPartial", ParamDef::CHECK, "true"},
    };
}
