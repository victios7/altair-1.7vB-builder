
#define _POSIX_C_SOURCE 200809L
#include "sema.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SYMS 8192
typedef struct { char name[128]; VType type; int is_fun; int is_class; } Sym;
static Sym g_syms[MAX_SYMS];
static int g_nsyms=0;
static int g_scope_start[256];
static int g_scope_depth=0;

static void scope_push(void){ g_scope_start[g_scope_depth++]=g_nsyms; }
static void scope_pop(void) { if(g_scope_depth>0) g_nsyms=g_scope_start[--g_scope_depth]; }
static void sym_push(const char *name,VType t,int fun,int cls){
    if(g_nsyms>=MAX_SYMS) return;
    strncpy(g_syms[g_nsyms].name,name,127); g_syms[g_nsyms].name[127]='\0';
    g_syms[g_nsyms].type=t; g_syms[g_nsyms].is_fun=fun; g_syms[g_nsyms].is_class=cls;
    g_nsyms++;
}
static int sym_exists(const char *name){
    for(int i=g_nsyms-1;i>=0;i--)
        if(strcmp(g_syms[i].name,name)==0) return 1;
    return 0;
}
static void sema_err(const char *code,const char *msg,int line){
    fprintf(stderr,"\n%s\n\n%s\n\nLine: %d\n",code,msg,line);
    exit(1);
}
static void sema_warn(const char *name, int line){
    fprintf(stderr,"[WARN] Line %d: variable '%s' used but not declared.\n",line,name);
}

static void check_node(ASTNode *n, int in_fun, int in_class);
static void check_block(ASTNode *blk, int in_fun, int in_class){
    if(!blk) return;
    if(blk->kind==ND_BLOCK){
        for(int i=0;i<blk->nchildren;i++) check_node(blk->children[i],in_fun,in_class);
    } else {
        check_node(blk,in_fun,in_class);
    }
}

static void check_node(ASTNode *n, int in_fun, int in_class){
    if(!n) return;
    switch(n->kind){
    case ND_HEADER: break;

    case ND_VAR_DECL:
        if(n->weight<0)
            sema_err("ALT0008","Weight must be a non-negative integer.",n->line);
        if(n->storage==STOR_PREFER && n->nprefer==0)
            sema_err("ALT0006","Prefer block must have at least one storage entry.",n->line);
        if(n->storage==STOR_ORBIT){
            for(int i=0;i<n->norbit;i++)
                for(int j=i+1;j<n->norbit;j++)
                    if(n->orbit[i].state_num==n->orbit[j].state_num)
                        sema_err("ALT0009","Orbit has duplicate state numbers.",n->line);
        }
        sym_push(n->var_name, n->var_type, 0, 0);
        if(n->nchildren>0) check_node(n->children[0],in_fun,in_class);
        break;

    case ND_FUN_DECL:
        sym_push(n->fun_name, n->return_type, 1, 0);
        scope_push();
        for(int i=0;i<n->nparam;i++) sym_push(n->param_names[i],n->param_types[i],0,0);
        check_block(n->fun_body,1,in_class);
        scope_pop();
        break;

    case ND_CLASS_DECL:
        sym_push(n->class_name,VTYPE_OBJECT,0,1);
        scope_push();
        for(int i=0;i<n->nchildren;i++) check_node(n->children[i],in_fun,1);
        scope_pop();
        break;

    case ND_CHOOSE:
        sym_push(n->choose_name,VTYPE_TEXT,0,0);
        break;

    case ND_FOREACH:
        sym_push(n->iter_var,VTYPE_TEXT,0,0);
        check_node(n->iter_list_expr,in_fun,in_class);
        check_block(n->body,in_fun,in_class);
        break;

    case ND_TRY_CATCH:
        check_block(n->try_body,in_fun,in_class);
        sym_push(n->catch_ident,VTYPE_TEXT,0,0);
        { char tmp[192];
          snprintf(tmp,sizeof(tmp),"%s_code",n->catch_ident); sym_push(tmp,VTYPE_TEXT,0,0);
          snprintf(tmp,sizeof(tmp),"%s_message",n->catch_ident); sym_push(tmp,VTYPE_TEXT,0,0);
          snprintf(tmp,sizeof(tmp),"%s_line",n->catch_ident); sym_push(tmp,VTYPE_NUMERIC,0,0);
        }
        check_block(n->catch_body,in_fun,in_class);
        break;

    case ND_MIGRATE:
        if(n->migrate_var[0] && !sym_exists(n->migrate_var))
            sema_warn(n->migrate_var, n->line);
        break;

    case ND_RELEASE:
        if(n->release_var[0] && !sym_exists(n->release_var))
            sema_warn(n->release_var, n->line);
        break;

    case ND_BINOP:
        check_node(n->left,in_fun,in_class); check_node(n->right,in_fun,in_class); break;
    case ND_UNOP:
        check_node(n->right,in_fun,in_class); break;

    case ND_RETURN: case ND_LOG: case ND_EXPR_STMT:
        for(int i=0;i<n->nchildren;i++) check_node(n->children[i],in_fun,in_class);
        check_node(n->left,in_fun,in_class);
        check_node(n->right,in_fun,in_class);
        break;

    case ND_IF:
        for(int i=0;i<n->nchildren;i++) check_node(n->children[i],in_fun,in_class);
        break;

    case ND_WHILE: case ND_REPEAT: case ND_FOREVER:
        check_node(n->count_expr,in_fun,in_class);
        check_block(n->body,in_fun,in_class);
        break;

    case ND_FUNC_CALL: case ND_METHOD_CALL: case ND_OBJECT_CREATE:
        for(int i=0;i<n->nchildren;i++) check_node(n->children[i],in_fun,in_class);
        break;

    case ND_ASSIGN: case ND_COMPOUND_ASSIGN: case ND_INDEX_ASSIGN:
        check_node(n->left,in_fun,in_class);
        check_node(n->right,in_fun,in_class);
        check_node(n->idx_expr,in_fun,in_class);
        check_node(n->idx_val,in_fun,in_class);
        break;

    case ND_IDENT:
        if(n->str_val[0] && !sym_exists(n->str_val) &&
           strcmp(n->str_val,"system")!=0 &&
           strcmp(n->str_val,"compiler")!=0 &&
           strcmp(n->str_val,"program")!=0)
        {   char _m[1200]; snprintf(_m,sizeof(_m),"Variable '%s' is used but was never declared.",n->str_val);
            sema_err("ALT0016",_m,n->line); }
        break;

    case ND_INTROSPECT:
        if(strcmp(n->introspect_ns,"system")!=0&&
           strcmp(n->introspect_ns,"compiler")!=0&&
           strcmp(n->introspect_ns,"program")!=0){
            char m[128]; snprintf(m,sizeof(m),"Unknown namespace '%s'.",n->introspect_ns);
            sema_err("ALT0011",m,n->line);
        }
        break;

    case ND_SNAPSHOT:
        if(n->snap_op[0]=='\0')
            sema_err("ALT0012","Snapshot requires create/restore/delete.",n->line);
        break;

    case ND_LISTEN:
        check_block(n->body,0,0);
        break;

    case ND_ROUTE:
        sym_push(n->handler_name,VTYPE_VOID,1,0);
        check_block(n->body,1,0);
        break;

    case ND_MIDDLEWARE:
        sym_push(n->handler_name,VTYPE_VOID,1,0);
        check_block(n->body,1,0);
        break;

    case ND_JOB:
        sym_push(n->handler_name,VTYPE_VOID,1,0);
        check_block(n->body,1,0);
        break;

    case ND_HEALTH:
        for(int i=0;i<n->nchildren;i++) check_node(n->children[i],in_fun,in_class);
        break;

    case ND_METRICS:
        break;

    case ND_ON_SHUTDOWN:
        check_block(n->body,1,0);
        break;

    case ND_SESSION_DECL:
        sym_push(n->session_var,VTYPE_TEXT,0,0);
        break;

    case ND_CONFIG_DECL:
        check_block(n->body,in_fun,in_class);
        break;

    case ND_DB_POOL:
        sym_push(n->db_var,VTYPE_TEXT,0,0);
        break;

    case ND_RESPOND_JSON: case ND_RESPOND_TEXT: case ND_RESPOND_STATUS:
        for(int i=0;i<n->nchildren;i++) check_node(n->children[i],in_fun,in_class);
        break;

    case ND_LINK: break;

    case ND_WINDOW_DECL:
        for(int i=0;i<n->ngfx_props;i++) check_node(n->gfx_vals[i],in_fun,in_class);
        break;

    case ND_LOOP:
        check_block(n->body,in_fun,in_class);
        break;

    case ND_DRAW_CMD:
        for(int i=0;i<n->ngfx_props;i++) check_node(n->gfx_vals[i],in_fun,in_class);
        break;

    case ND_CLEAR_STMT:
        check_node(n->left,in_fun,in_class);
        break;

    case ND_COLOR_DECL:
        sym_push(n->var_name,VTYPE_COLOR,0,0);
        for(int i=0;i<n->nchildren;i++) check_node(n->children[i],in_fun,in_class);
        break;

    case ND_IMAGE_DECL:
        sym_push(n->var_name,VTYPE_IMAGE,0,0);
        break;

    case ND_SOUND_DECL:
        sym_push(n->var_name,VTYPE_SOUND,0,0);
        break;

    case ND_MUSIC_DECL:
        sym_push(n->var_name,VTYPE_MUSIC,0,0);
        break;

    case ND_PLAY_STMT: case ND_STOP_STMT: case ND_PAUSE_STMT:
        break;

    case ND_TIMER_DECL:
        sym_push(n->var_name,VTYPE_NUMERIC,0,0);
        for(int i=0;i<n->nchildren;i++) check_node(n->children[i],in_fun,in_class);
        break;

    case ND_WIDGET_DECL:
        if(n->var_name[0]) sym_push(n->var_name,VTYPE_OBJECT,0,0);
        for(int i=0;i<n->ngfx_props;i++) check_node(n->gfx_vals[i],in_fun,in_class);
        break;

    case ND_MENU_DECL:
        if(n->var_name[0]) sym_push(n->var_name,VTYPE_OBJECT,0,0);
        for(int i=0;i<n->ngfx_props;i++) check_node(n->gfx_vals[i],in_fun,in_class);
        break;

    case ND_DIALOG_DECL: case ND_POPUP_DECL: case ND_CANVAS_GFX:
        if(n->var_name[0]) sym_push(n->var_name,VTYPE_OBJECT,0,0);
        check_block(n->body,in_fun,in_class);
        break;

    case ND_SCENE_DECL:
        if(n->var_name[0]) sym_push(n->var_name,VTYPE_TEXT,0,0);
        break;

    case ND_GOTO_STMT: break;
    case ND_CURSOR_STMT: break;

    case ND_ANIMATE_DECL:
        for(int i=0;i<n->ngfx_props;i++) check_node(n->gfx_vals[i],in_fun,in_class);
        break;

    case ND_LAYOUT:
        check_block(n->body,in_fun,in_class);
        break;

    case ND_KEY_EXPR: break;

    case ND_BLOCK:
        for(int i=0;i<n->nchildren;i++) check_node(n->children[i],in_fun,in_class);
        break;

    case ND_NUMBER: case ND_STRING: case ND_BOOL:
    case ND_LIST_LIT: case ND_INDEX_ACCESS: case ND_MEMBER_ACCESS:
    case ND_WAIT: case ND_EXIT: case ND_CALL_GALAXY:
    case ND_USER_INPUT: case ND_PROGRAM:
    default:
        break;
    }
}

void sema_check(ASTNode *program){
    g_nsyms=0; g_scope_depth=0;
    if(!program) return;

    static const char *color_names[]={
        "white","black","red","green","blue","yellow","orange",
        "purple","pink","gray","lightgray","darkgray","brown",
        "skyblue","darkblue","maroon","darkgreen","lime","gold",
        "beige","magenta","violet","darkpurple","darkbrown",
        "raywhite","transparent",NULL
    };
    for(int i=0;color_names[i];i++) sym_push(color_names[i],VTYPE_NUMERIC,0,0);

    sym_push("MOUSE_LEFT",  VTYPE_NUMERIC,0,0);
    sym_push("MOUSE_RIGHT", VTYPE_NUMERIC,0,0);
    sym_push("MOUSE_MIDDLE",VTYPE_NUMERIC,0,0);

    for(int i=0;i<program->nchildren;i++) check_node(program->children[i],0,0);
}

