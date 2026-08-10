
#define _POSIX_C_SOURCE 200809L
#include "codegen.h"
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>

typedef struct {
    FILE *fp;
    int   indent;
    int   tmp;
    char  cur_class[128];
    char  class_fields[32][128];
    int   nclass_fields;
    int   in_method;
    int   loop_depth;
    char  known_classes[64][128];
    int   nknown;
    char  catch_ids[16][64];
    int   ncatch_ids;
    char  var_names[256][128];
    char  var_class[256][128];
    int   nvar_class;
    char  token_vars[256][128];
    int   ntoken_vars;
    const char *source_file;

    char  local_vars[256][128];
    int   nlocal;
    int   in_fun;
    char  known_funs[128][128];
    int   nknown_funs;

    int   use_raylib;
    int   gfx_scene_count;

    char  persist_names[64][128];
    char  persist_files[64][256];
    int   npersist;
} CG;

static char *cg_fmt(const char *fmt,...);
static char *c_escape(const char *s);
static char *cg_expr(CG *g, ASTNode *n);

static const struct { const char *alt; const char *rl; } RL_COLORS[] = {
    {"white","WHITE"},{"black","BLACK"},{"red","RED"},{"green","GREEN"},
    {"blue","BLUE"},{"yellow","YELLOW"},{"orange","ORANGE"},{"purple","PURPLE"},
    {"pink","PINK"},{"gray","GRAY"},{"grey","GRAY"},{"lightgray","LIGHTGRAY"},
    {"darkgray","DARKGRAY"},{"brown","BROWN"},{"skyblue","SKYBLUE"},
    {"darkblue","DARKBLUE"},{"maroon","MAROON"},{"darkgreen","DARKGREEN"},
    {"lime","LIME"},{"gold","GOLD"},{"beige","BEIGE"},{"magenta","MAGENTA"},
    {"violet","VIOLET"},{"darkpurple","DARKPURPLE"},{"darkbrown","DARKBROWN"},
    {"raywhite","RAYWHITE"},{"transparent","BLANK"},{NULL,NULL}
};

static const char *rl_color(const char *s){
    for(int i=0;RL_COLORS[i].alt;i++) if(strcasecmp(s,RL_COLORS[i].alt)==0) return RL_COLORS[i].rl;
    return NULL;
}

static const struct { const char *alt; const char *rl; } RL_KEYS[] = {
    {"SPACE","KEY_SPACE"},{"ENTER","KEY_ENTER"},{"ESCAPE","KEY_ESCAPE"},
    {"BACKSPACE","KEY_BACKSPACE"},{"TAB","KEY_TAB"},
    {"RIGHT","KEY_RIGHT"},{"LEFT","KEY_LEFT"},{"UP","KEY_UP"},{"DOWN","KEY_DOWN"},
    {"SHIFT","KEY_LEFT_SHIFT"},{"CTRL","KEY_LEFT_CONTROL"},{"ALT","KEY_LEFT_ALT"},
    {"A","KEY_A"},{"B","KEY_B"},{"C","KEY_C"},{"D","KEY_D"},{"E","KEY_E"},
    {"F","KEY_F"},{"G","KEY_G"},{"H","KEY_H"},{"I","KEY_I"},{"J","KEY_J"},
    {"K","KEY_K"},{"L","KEY_L"},{"M","KEY_M"},{"N","KEY_N"},{"O","KEY_O"},
    {"P","KEY_P"},{"Q","KEY_Q"},{"R","KEY_R"},{"S","KEY_S"},{"T","KEY_T"},
    {"U","KEY_U"},{"V","KEY_V"},{"W","KEY_W"},{"X","KEY_X"},{"Y","KEY_Y"},
    {"Z","KEY_Z"},
    {"0","KEY_ZERO"},{"1","KEY_ONE"},{"2","KEY_TWO"},{"3","KEY_THREE"},
    {"4","KEY_FOUR"},{"5","KEY_FIVE"},{"6","KEY_SIX"},{"7","KEY_SEVEN"},
    {"8","KEY_EIGHT"},{"9","KEY_NINE"},
    {"F1","KEY_F1"},{"F2","KEY_F2"},{"F3","KEY_F3"},{"F4","KEY_F4"},
    {"F5","KEY_F5"},{"F6","KEY_F6"},{"F7","KEY_F7"},{"F8","KEY_F8"},
    {"F9","KEY_F9"},{"F10","KEY_F10"},{"F11","KEY_F11"},{"F12","KEY_F12"},
    {NULL,NULL}
};
static const char *rl_key(const char *s){
    for(int i=0;RL_KEYS[i].alt;i++) if(strcasecmp(s,RL_KEYS[i].alt)==0) return RL_KEYS[i].rl;

    static char buf[32]; snprintf(buf,sizeof(buf),"KEY_%s",s);
    for(int i=0;buf[i];i++) buf[i]=(char)(buf[i]>='a'&&buf[i]<='z'?buf[i]-32:buf[i]);
    return buf;
}

static char *cg_rl_color_expr(CG *g, ASTNode *n){
    if(!n) return strdup("WHITE");
    if(n->kind==ND_IDENT){
        const char *c=rl_color(n->str_val);
        return c ? strdup(c) : strdup(n->str_val);
    }
    if(n->kind==ND_FUNC_CALL && strcmp(n->fun_name,"rgb")==0 && n->nchildren>=3){
        char *r=cg_expr(g,n->children[0]);
        char *gr=cg_expr(g,n->children[1]);
        char *b=cg_expr(g,n->children[2]);
        char a_buf[64]="255";
        if(n->nchildren>=4){ char *aa=cg_expr(g,n->children[3]);
            snprintf(a_buf,sizeof(a_buf),"(int)((%s)->num)",aa); free(aa); }
        char *res=cg_fmt("(Color){(int)((%s)->num),(int)((%s)->num),(int)((%s)->num),%s}",r,gr,b,a_buf);
        free(r); free(gr); free(b); return res;
    }
    if(n->kind==ND_STRING && n->str_val[0]=='#'){

        unsigned int rv=0,gv=0,bv=0,av=255;
        sscanf(n->str_val+1,"%2x%2x%2x%2x",&rv,&gv,&bv,&av);
        return cg_fmt("(Color){%u,%u,%u,%u}",rv,gv,bv,av);
    }
    return strdup("WHITE");
}

static char *cg_rl_int(CG *g, ASTNode *n){
    if(!n) return strdup("0");
    if(n->kind==ND_NUMBER) return cg_fmt("%d",(int)n->num_val);
    char *e=cg_expr(g,n);
    char *r=cg_fmt("(int)((%s)->num)",e);
    free(e); return r;
}

static char *cg_rl_float(CG *g, ASTNode *n){
    if(!n) return strdup("0.0f");

    if(n->kind==ND_NUMBER) return cg_fmt("%.1ff", n->num_val);
    char *e=cg_expr(g,n);
    char *r=cg_fmt("(float)((%s)->num)",e);
    free(e); return r;
}

static char *cg_rl_str(CG *g, ASTNode *n){
    if(!n) return strdup("\"\"");
    if(n->kind==ND_STRING){ char *esc=c_escape(n->str_val); char *r=cg_fmt("\"%s\"",esc); free(esc); return r; }
    char *e=cg_expr(g,n);
    char *r=cg_fmt("((%s)->str?(%s)->str:\"\")",e,e);
    free(e); return r;
}

static ASTNode *gfx_prop(ASTNode *n, const char *k){
    for(int i=0;i<n->ngfx_props;i++)
        if(strcmp(n->gfx_props[i],k)==0) return n->gfx_vals[i];
    return NULL;
}

static int is_catch_id(CG *g, const char *name){
    for(int i=0;i<g->ncatch_ids;i++) if(strcmp(g->catch_ids[i],name)==0) return 1;
    return 0;
}

static int is_local_var(CG *g, const char *name){
    for(int i=0;i<g->nlocal;i++) if(strcmp(g->local_vars[i],name)==0) return 1;
    return 0;
}
static void push_local(CG *g, const char *name){
    if(g->nlocal<256) strncpy(g->local_vars[g->nlocal++],name,127);
}
static void record_var_class(CG *g, const char *var, const char *cls){
    if(g->nvar_class>=256) return;
    strncpy(g->var_names[g->nvar_class],var,127);
    strncpy(g->var_class[g->nvar_class],cls,127);
    g->nvar_class++;
}
static void record_persist(CG *g, const char *var, const char *file){
    if(g->npersist>=64) return;
    strncpy(g->persist_names[g->npersist],var,127);
    strncpy(g->persist_files[g->npersist],file,255);
    g->npersist++;
}
static const char *find_persist_file(CG *g, const char *var){
    for(int i=0;i<g->npersist;i++) if(strcmp(g->persist_names[i],var)==0) return g->persist_files[i];
    return NULL;
}
static const char *lookup_var_class(CG *g, const char *var){
    for(int i=0;i<g->nvar_class;i++)
        if(strcmp(g->var_names[i],var)==0) return g->var_class[i];
    return NULL;
}
static void record_token_var(CG *g, const char *var){
    if(g->ntoken_vars>=256) return;
    strncpy(g->token_vars[g->ntoken_vars++],var,127);
}
static int is_token_var(CG *g, const char *var){
    for(int i=0;i<g->ntoken_vars;i++)
        if(strcmp(g->token_vars[i],var)==0) return 1;
    return 0;
}

static void ind(CG *g){ for(int i=0;i<g->indent;i++) fprintf(g->fp,"    "); }
static void emit(CG *g,const char *fmt,...){
    va_list ap; va_start(ap,fmt); vfprintf(g->fp,fmt,ap); va_end(ap);
}
static void emitln(CG *g,const char *fmt,...){
    ind(g); va_list ap; va_start(ap,fmt); vfprintf(g->fp,fmt,ap); va_end(ap);
    fprintf(g->fp,"\n");
}
static int newtmp(CG *g){ return g->tmp++; }
static char *cg_int_expr(CG *g, char *e){
    int t=newtmp(g);
    char *r=cg_fmt("({ AltairVal *_ie%d=%s; int _iv%d=(int)_ie%d->num; altair_val_free(_ie%d); _iv%d; })",t,e,t,t,t,t);
    free(e);
    return r;
}
static int is_known_class(CG *g,const char *name){
    for(int i=0;i<g->nknown;i++) if(strcmp(g->known_classes[i],name)==0) return 1;
    return 0;
}
static int is_known_fun(CG *g,const char *name){
    for(int i=0;i<g->nknown_funs;i++) if(strcmp(g->known_funs[i],name)==0) return 1;
    return 0;
}
static int is_field(CG *g,const char *name){
    for(int i=0;i<g->nclass_fields;i++) if(strcmp(g->class_fields[i],name)==0) return 1;
    return 0;
}

static void emit_line(CG *g, int line){
    if(!g->source_file || line<=0) return;
    char safe[1024]; int si=0;
    for(int i=0; g->source_file[i] && si<1022; i++)
        safe[si++] = (g->source_file[i]=='\\') ? '/' : g->source_file[i];
    safe[si]='\0';
    fprintf(g->fp,"#line %d \"%s\"\n", line, safe);
}

static const char *stor_c(StorKind s){
    switch(s){
    case STOR_RAM:  return "ALT_RAM";
    case STOR_DISK: return "ALT_DISK";
    case STOR_CACHE:return "ALT_CACHE";
    case STOR_TEMP: return "ALT_TEMP";
    default:        return "ALT_AUTO";
    }
}
static const char *vtype_c(VType t){
    switch(t){
    case VTYPE_NUMERIC:return "ALT_NUMERIC"; case VTYPE_TEXT:return "ALT_TEXT";
    case VTYPE_BOOL:return "ALT_BOOL";       case VTYPE_LIST:return "ALT_LIST";
    case VTYPE_OBJECT:return "ALT_OBJECT";   case VTYPE_TOKEN:return "ALT_TOKEN";
    case VTYPE_FILE:return "ALT_FILE";       case VTYPE_POINTER:return "ALT_POINTER";
    default:return "ALT_VOID";
    }
}

static char *cg_expr(CG *g, ASTNode *n);
static void  cg_stmt(CG *g, ASTNode *n);
static void  cg_block(CG *g, ASTNode *blk);

static char *cg_fmt(const char *fmt,...){
    char buf[4096]; va_list ap; va_start(ap,fmt); vsnprintf(buf,sizeof(buf),fmt,ap); va_end(ap);
    return strdup(buf);
}

static char *c_escape(const char *s){
    char buf[4096]; int bi=0;
    for(int i=0;s[i]&&bi<4090;i++){
        if(s[i]=='"')       { buf[bi++]='\\'; buf[bi++]='"'; }
        else if(s[i]=='\\') { buf[bi++]='\\'; buf[bi++]='\\'; }
        else if(s[i]=='\n') { buf[bi++]='\\'; buf[bi++]='n'; }
        else if(s[i]=='\t') { buf[bi++]='\\'; buf[bi++]='t'; }
        else                  buf[bi++]=s[i];
    }
    buf[bi]='\0'; return strdup(buf);
}

static char *cg_expr(CG *g, ASTNode *n);
static char *cg_expr_receiver(CG *g, ASTNode *n){
    if(n && n->kind==ND_IDENT){
        const char *name=n->str_val;
        if(g->in_method && is_field(g,name))
            return cg_fmt("altair_obj_get(_self,\"%s\",%d)", name, n->line);
        if(is_token_var(g,name))
            return cg_fmt("altair_token_use(%s_var,%d)", name, n->line);
        if(g->in_fun && !is_local_var(g,name) && !is_catch_id(g,name))
            return cg_fmt("altair_var_get(altair_var_lookup(\"%s\"))", name);
        return cg_fmt("altair_var_get(%s_var)", name);
    }
    return cg_expr(g,n);
}

static char *cg_expr(CG *g, ASTNode *n){
    if(!n) return strdup("altair_num(0.0)");
    switch(n->kind){
    case ND_NUMBER: return cg_fmt("altair_num(%g)", n->num_val);
    case ND_STRING: {
        char *esc=c_escape(n->str_val);
        char *r=cg_fmt("altair_str(\"%s\")",esc); free(esc); return r;
    }
    case ND_BOOL:   return cg_fmt("altair_bool(%d)", n->bool_val);

    case ND_IDENT: {
        const char *name=n->str_val;

        if(g->in_method && is_field(g,name))
            return cg_fmt("altair_val_copy(altair_obj_get(_self,\"%s\",%d))", name, n->line);

        if(is_token_var(g,name))
            return cg_fmt("altair_token_use(%s_var,%d)", name, n->line);

        if(g->in_fun && !is_local_var(g,name) && !is_catch_id(g,name)){
            return cg_fmt("altair_val_copy(altair_var_get(altair_var_lookup(\"%s\")))", name);
        }
        return cg_fmt("altair_val_copy(altair_var_get(%s_var))", name);
    }

    case ND_BINOP: {
        char *l=cg_expr(g,n->left);
        char *r=cg_expr(g,n->right);
        char *res;

        switch(n->op){
        case TOK_PLUS:  res=cg_fmt("altair_add(%s,%s,%d)",l,r,n->line); break;
        case TOK_MINUS: res=cg_fmt("altair_sub(%s,%s,%d)",l,r,n->line); break;
        case TOK_STAR:  res=cg_fmt("altair_mul(%s,%s,%d)",l,r,n->line); break;
        case TOK_SLASH: res=cg_fmt("altair_div(%s,%s,%d)",l,r,n->line); break;
        case TOK_PERCENT_LIT:
        case TOK_PERCENT:res=cg_fmt("altair_mod(%s,%s,%d)",l,r,n->line); break;
        case TOK_EQ:    res=cg_fmt("altair_eq(%s,%s)",l,r); break;
        case TOK_NEQ:   res=cg_fmt("altair_neq(%s,%s)",l,r); break;
        case TOK_LT:    res=cg_fmt("altair_lt(%s,%s,%d)",l,r,n->line); break;
        case TOK_GT:    res=cg_fmt("altair_gt(%s,%s,%d)",l,r,n->line); break;
        case TOK_LTE:   res=cg_fmt("altair_lte(%s,%s,%d)",l,r,n->line); break;
        case TOK_GTE:   res=cg_fmt("altair_gte(%s,%s,%d)",l,r,n->line); break;
        case TOK_AND:   res=cg_fmt("altair_and(%s,%s)",l,r); break;
        case TOK_OR:    res=cg_fmt("altair_or(%s,%s)",l,r); break;

        case TOK_AMP:   res=cg_fmt("altair_band(%s,%s,%d)",l,r,n->line); break;
        case TOK_PIPE:  res=cg_fmt("altair_bor(%s,%s,%d)",l,r,n->line); break;
        case TOK_CARET: res=cg_fmt("altair_bxor(%s,%s,%d)",l,r,n->line); break;
        case TOK_SHL:   res=cg_fmt("altair_shl(%s,%s,%d)",l,r,n->line); break;
        case TOK_SHR:   res=cg_fmt("altair_shr(%s,%s,%d)",l,r,n->line); break;

        default:        res=strdup("altair_num(0.0)"); break;
        }
        free(l); free(r); return res;
    }

    case ND_UNOP: {
        char *e=cg_expr(g,n->right);
        char *res;
        if(n->op==TOK_BANG) res=cg_fmt("altair_not(%s)",e);
        else if(n->op==TOK_TILDE) res=cg_fmt("altair_bnot(%s,%d)",e,n->line);
        else res=cg_fmt("altair_neg(%s)",e);
        free(e); return res;
    }

    case ND_LIST_LIT: {
        int t=newtmp(g);
        emitln(g,"AltairVal *_lst%d = altair_list_new();",t);
        for(int i=0;i<n->nchildren;i++){
            char *e=cg_expr(g,n->children[i]);
            emitln(g,"{ AltairVal *_li%d_%d=%s; altair_list_append(_lst%d,_li%d_%d); altair_val_free(_li%d_%d); }",t,i,e,t,t,i,t,i);
            free(e);
        }
        return cg_fmt("_lst%d",t);
    }

    case ND_FUNC_CALL: {
        if(is_known_class(g,n->fun_name)){
            return cg_fmt("altair_val_from_obj(_class_%s_new())", n->fun_name);
        }
        int ptr_mutating = strcmp(n->fun_name,"p_free")==0 || strcmp(n->fun_name,"p_write")==0
            || strcmp(n->fun_name,"close")==0
            || strcmp(n->fun_name,"read")==0
            || strcmp(n->fun_name,"read_line")==0
            || strcmp(n->fun_name,"write")==0;
        char args[2048]="";
        for(int i=0;i<n->nchildren;i++){
            char *e = (ptr_mutating && i==0) ? cg_expr_receiver(g,n->children[i]) : cg_expr(g,n->children[i]);
            if(i>0) strncat(args,",",sizeof(args)-strlen(args)-1);
            strncat(args,e,sizeof(args)-strlen(args)-1);
            free(e);
        }
        return cg_fmt("_fn_%s(%s)", n->fun_name, args);
    }

    case ND_OBJECT_CREATE: {
        return cg_fmt("altair_val_from_obj(_class_%s_new())", n->str_val);
    }

    case ND_METHOD_CALL: {

        int mutating = strcmp(n->fun_name,"append")==0 ||
                       strcmp(n->fun_name,"remove")==0 ||
                       strcmp(n->fun_name,"clear")==0;
        char *obj = mutating ? cg_expr_receiver(g,n->left) : cg_expr(g,n->left);
        char args[2048]="";
        for(int i=0;i<n->nchildren;i++){
            char *e=cg_expr(g,n->children[i]);
            if(i>0) strncat(args,",",sizeof(args)-strlen(args)-1);
            strncat(args,e,sizeof(args)-strlen(args)-1);
            free(e);
        }
        if(strcmp(n->fun_name,"append")==0){
            char *r;
            if(n->nchildren==1)
                r=cg_fmt("({ AltairVal *_ai=%s; altair_list_append(%s,_ai); altair_val_free(_ai); (AltairVal*)NULL; })",args,obj);
            else
                r=cg_fmt("(altair_list_append(%s,%s),(AltairVal*)NULL)",obj,args);
            free(obj); return r;
        }
        if(strcmp(n->fun_name,"remove")==0){
            char *ie=cg_int_expr(g,strdup(args));
            char *r=cg_fmt("altair_bool(altair_list_remove(%s,%s))",obj,ie);
            free(obj); free(ie); return r;
        }
        if(strcmp(n->fun_name,"clear")==0){
            char *r=cg_fmt("(altair_list_clear(%s),(AltairVal*)NULL)",obj);
            free(obj); return r;
        }

        if(strcmp(n->fun_name,"length")==0 && n->nchildren==0){
            char *r=cg_fmt("altair_num(altair_list_length(%s))",obj);
            free(obj); return r;
        }

        if(strcmp(n->fun_name,"json")==0){
            char *r=cg_fmt("(altair_respond_json(_altair_res,%s),(AltairVal*)NULL)",args);
            free(obj); return r;
        }
        if(strcmp(n->fun_name,"text")==0){
            char *r=cg_fmt("(altair_respond_text(_altair_res,%s),(AltairVal*)NULL)",args);
            free(obj); return r;
        }
        if(strcmp(n->fun_name,"status")==0){
            char *ie=cg_int_expr(g,strdup(args));
            char *r=cg_fmt("(altair_respond_status(_altair_res,%s),(AltairVal*)NULL)",ie);
            free(obj); free(ie); return r;
        }

        if(strcmp(n->fun_name,"param")==0){
            char *r=cg_fmt("altair_req_param(_altair_req,%s->str)",args);
            free(obj); return r;
        }
        if(strcmp(n->fun_name,"header")==0){
            char *r=cg_fmt("altair_req_header(_altair_req,%s->str)",args);
            free(obj); return r;
        }
        if(strcmp(n->fun_name,"body")==0){
            char *r=cg_fmt("altair_req_body(_altair_req)");
            free(obj); return r;
        }
        const char *cls = NULL;
        if(n->left && n->left->kind==ND_IDENT)
            cls = lookup_var_class(g, n->left->str_val);
        char *r;
        if(cls){
            r=cg_fmt("_class_%s_method_%s((%s)->type==ALT_OBJECT?(%s)->obj:NULL%s%s)",
                     cls, n->fun_name, obj, obj, args[0]?",":"", args);
        } else {
            r=cg_fmt("((void)0 /* unknown class method %s */,(AltairVal*)NULL)",n->fun_name);
        }
        free(obj); return r;
    }

    case ND_MEMBER_ACCESS: {
        if(n->left && n->left->kind==ND_IDENT && is_catch_id(g,n->left->str_val)){
            const char *field=n->str_val;
            if(strcmp(field,"code")==0||strcmp(field,"message")==0||strcmp(field,"line")==0)
                return cg_fmt("altair_val_copy(altair_var_get(%s_%s_var))",n->left->str_val,field);
        }
        char *obj=cg_expr(g,n->left);

        if(strcmp(n->str_val,"length")==0){
            char *r=cg_fmt("altair_num(altair_list_length(%s))",obj);
            free(obj); return r;
        }
        char *r=cg_fmt("altair_val_copy(altair_obj_get((%s)->type==ALT_OBJECT?(%s)->obj:NULL,\"%s\",%d))",
                       obj,obj,n->str_val,n->line);
        free(obj); return r;
    }

    case ND_INDEX_ACCESS: {
        char *ie=cg_int_expr(g,cg_expr(g,n->idx_expr));
        char *r;

        if(g->in_fun && !is_local_var(g,n->idx_list)){
            r=cg_fmt("altair_val_copy(altair_list_get(altair_var_get(altair_var_lookup(\"%s\")),%s,%d))",
                     n->idx_list, ie, n->line);
        } else {
            r=cg_fmt("altair_val_copy(altair_list_get(altair_var_get(%s_var),%s,%d))",
                     n->idx_list, ie, n->line);
        }
        free(ie); return r;
    }

    case ND_INTROSPECT: {

        if(strcmp(n->introspect_ns,"system")==0){
            if(n->introspect_var[0])
                return cg_fmt("altair_var_system(%s_var,\"%s\")",
                              n->introspect_var, n->introspect_key);
            return cg_fmt("altair_system(\"%s\")", n->introspect_key);
        }
        if(strcmp(n->introspect_ns,"compiler")==0)
            return cg_fmt("altair_compiler(\"%s\")", n->introspect_key);
        if(strcmp(n->introspect_ns,"program")==0)
            return cg_fmt("altair_program(\"%s\")", n->introspect_key);
        return strdup("altair_num(0.0)");
    }

    case ND_USER_INPUT: {
        char *esc=c_escape(n->input_prompt);
        char *r=cg_fmt("altair_user_input(\"%s\",%s)", esc, vtype_c(n->input_type));
        free(esc); return r;
    }

    case ND_KEY_EXPR: {
        const char *k=rl_key(n->str_val);
        return cg_fmt("altair_bool(IsKeyDown(%s))",k);
    }

    default: return strdup("altair_num(0.0)");
    }
}

static void cg_block(CG *g, ASTNode *blk){
    if(!blk) return;
    for(int i=0;i<blk->nchildren;i++) cg_stmt(g,blk->children[i]);
}

static void cg_stmt(CG *g, ASTNode *n){
    if(!n) return;
    if(n->line>0) emit_line(g, n->line);

    switch(n->kind){

    case ND_HEADER: break;

    case ND_VAR_DECL: {
        const char *vname=n->var_name;
        const char *vt=vtype_c(n->var_type);
        int is_const=n->is_const;
        double esecs=n->expire_secs;
        int wt=n->weight;

        if(g->in_fun) push_local(g, vname);

        if(n->storage==STOR_ORBIT){
            emitln(g,"AltOrbitEntry _%s_orbit[] = {",vname);
            g->indent++;
            for(int i=0;i<n->norbit;i++){
                OrbitEntry *e=&n->orbit[i];
                emitln(g,"{%d,\"%s\",%s},",e->state_num,e->state_name,stor_c(e->storage));
            }
            g->indent--;
            emitln(g,"};");
            emitln(g,"AltairVar *%s_var = altair_var_orbit(\"%s\",%s,_%s_orbit,%d);",
                   vname,vname,vt,vname,n->norbit);
        } else if(n->storage==STOR_PREFER){
            emitln(g,"{ AltStorage _%s_prefer[] = {",vname);
            for(int i=0;i<n->nprefer;i++) emit(g,"%s,",stor_c(n->prefer[i].storage));
            emit(g,"\n");
            emitln(g,"AltairVar *%s_var = altair_var_prefer(\"%s\",%s,_%s_prefer,%d); }",
                   vname,vname,vt,vname,n->nprefer);

            emitln(g,"AltairVar *%s_var = altair_var_lookup(\"%s\");",vname,vname);
        } else {
            emitln(g,"AltairVar *%s_var = altair_var_new(\"%s\",%s,%s,%d,%d,%.1f);",
                   vname,vname,vt,stor_c(n->storage),is_const,wt,esecs);
        }

        if(n->var_type==VTYPE_OBJECT && n->nchildren>0){
            ASTNode *init=n->children[0];
            if(init->kind==ND_FUNC_CALL && is_known_class(g,init->fun_name))
                record_var_class(g,vname,init->fun_name);
        }
        if(n->var_type==VTYPE_TOKEN)
            record_token_var(g,vname);

        if(n->nchildren>0){
            ASTNode *init=n->children[0];
            if(n->var_type==VTYPE_TOKEN){
                char *e=cg_expr(g,init);

                emitln(g,"altair_var_set_own(%s_var, altair_token_new(%s));",vname,e);
                free(e);
            } else {
                char *e=cg_expr(g,init);
                if(n->var_type==VTYPE_NUMERIC && init->kind!=ND_NUMBER &&
                   init->kind!=ND_BINOP && init->kind!=ND_UNOP &&
                   init->kind!=ND_FUNC_CALL){
                    emitln(g,"{ AltairVal *_cn%d=altair_coerce_num(%s,%d); altair_var_set_own(%s_var,_cn%d); }",
                           g->tmp,e,n->line,vname,g->tmp); g->tmp++;
                } else {

                    emitln(g,"altair_var_set_own(%s_var, %s);",vname,e);
                }
                free(e);
            }
        }
        if(n->storage==STOR_DISK||n->storage==STOR_CACHE){
            emitln(g,"if(!altair_var_get(%s_var)) altair_var_set_own(%s_var,altair_num(0.0));",
                   vname,vname);
        }
        if(n->persist_file[0]){
            emitln(g,"{ AltairVal *_pl=altair_persist_load(\"%s\"); if(_pl) altair_var_set_own(%s_var,_pl); else altair_persist_save(\"%s\",altair_var_get(%s_var)); }",
                   n->persist_file,vname,n->persist_file,vname);
            emitln(g,"altair_persist_register(\"%s\",%s_var);",n->persist_file,vname);
            record_persist(g,vname,n->persist_file);
        }
        break;
    }

    case ND_ASSIGN: {
        if(n->left && n->left->kind==ND_IDENT){
            const char *vname=n->left->str_val;
            if(g->in_method && is_field(g,vname)){
                char *e=cg_expr(g,n->right);
                emitln(g,"{ AltairVal *_as%d=%s; altair_obj_set(_self,\"%s\",_as%d,%d); altair_val_free(_as%d); }",
                       g->tmp,e,vname,g->tmp,n->line,g->tmp); g->tmp++;
                free(e);
            } else if(g->in_fun && !is_local_var(g,vname)){

                char *e=cg_expr(g,n->right);
                emitln(g,"{ AltairVar *_gv%d=altair_var_lookup(\"%s\"); altair_var_set_own(_gv%d,%s); }",
                       g->tmp,vname,g->tmp,e);
                {
                    const char *pf=find_persist_file(g,vname);
                    if(pf) emitln(g,"altair_persist_save(\"%s\",altair_var_get(altair_var_lookup(\"%s\")));",pf,vname);
                }
                g->tmp++;
                free(e);
            } else {
                char *e=cg_expr(g,n->right);

                emitln(g,"altair_var_set_own(%s_var, %s);",vname,e);
                {
                    const char *pf=find_persist_file(g,vname);
                    if(pf) emitln(g,"altair_persist_save(\"%s\",altair_var_get(%s_var));",pf,vname);
                }
                free(e);
            }
        } else if(n->left && n->left->kind==ND_MEMBER_ACCESS){
            char *obj=cg_expr(g,n->left->left);
            char *val=cg_expr(g,n->right);
            emitln(g,"{ AltairVal *_mo%d=%s; altair_obj_set((%s)->type==ALT_OBJECT?(%s)->obj:NULL,\"%s\",_mo%d,%d); altair_val_free(_mo%d); }",
                   g->tmp,val,obj,obj,n->left->str_val,g->tmp,n->line,g->tmp); g->tmp++;
            free(obj); free(val);
        } else {
            char *l=cg_expr(g,n->left);
            char *r=cg_expr(g,n->right);
            emitln(g,"/* assign */ (void)(%s); (void)(%s);",l,r);
            free(l); free(r);
        }
        break;
    }

    case ND_COMPOUND_ASSIGN: {
        const char *vname = n->var_name[0] ? n->var_name :
                            (n->left&&n->left->kind==ND_IDENT ? n->left->str_val : "");
        if(!vname[0]) break;
        char *rhs=cg_expr(g,n->right);
        if(g->in_fun && !is_local_var(g,vname)){

            int t=newtmp(g);
            emitln(g,"{ AltairVar *_gcv%d=altair_var_lookup(\"%s\");",t,vname);
            g->indent++;
            switch(n->compound_op){
            case TOK_PLUS_ASSIGN:
                emitln(g,"if(_gcv%d) altair_var_set_own(_gcv%d,altair_add(altair_var_get(_gcv%d),%s,%d));",t,t,t,rhs,n->line); break;
            case TOK_MINUS_ASSIGN:
                emitln(g,"if(_gcv%d) altair_var_set_own(_gcv%d,altair_sub(altair_var_get(_gcv%d),%s,%d));",t,t,t,rhs,n->line); break;
            case TOK_STAR_ASSIGN:
                emitln(g,"if(_gcv%d) altair_var_set_own(_gcv%d,altair_mul(altair_var_get(_gcv%d),%s,%d));",t,t,t,rhs,n->line); break;
            case TOK_SLASH_ASSIGN:
                emitln(g,"if(_gcv%d) altair_var_set_own(_gcv%d,altair_div(altair_var_get(_gcv%d),%s,%d));",t,t,t,rhs,n->line); break;
            default:
                emitln(g,"if(_gcv%d) altair_var_set_own(_gcv%d,altair_mod(altair_var_get(_gcv%d),%s,%d));",t,t,t,rhs,n->line); break;
            }
            g->indent--;
            emitln(g,"}");
        } else {
            switch(n->compound_op){
            case TOK_PLUS_ASSIGN:
                emitln(g,"altair_var_set_own(%s_var, altair_add(altair_var_get(%s_var),%s,%d));",vname,vname,rhs,n->line); break;
            case TOK_MINUS_ASSIGN:
                emitln(g,"altair_var_set_own(%s_var, altair_sub(altair_var_get(%s_var),%s,%d));",vname,vname,rhs,n->line); break;
            case TOK_STAR_ASSIGN:
                emitln(g,"altair_var_set_own(%s_var, altair_mul(altair_var_get(%s_var),%s,%d));",vname,vname,rhs,n->line); break;
            case TOK_SLASH_ASSIGN:
                emitln(g,"altair_var_set_own(%s_var, altair_div(altair_var_get(%s_var),%s,%d));",vname,vname,rhs,n->line); break;
            default:
                emitln(g,"altair_var_set_own(%s_var, altair_mod(altair_var_get(%s_var),%s,%d));",vname,vname,rhs,n->line); break;
            }
        }
        free(rhs);
        break;
    }

    case ND_INDEX_ASSIGN: {
        char *ie=cg_expr(g,n->idx_expr);
        char *val=cg_expr(g,n->idx_val);
        if(g->in_fun && !is_local_var(g,n->idx_list)){
            emitln(g,"{ AltairVal *_glst=altair_var_get(altair_var_lookup(\"%s\")); AltairVal *_ixv=%s; altair_list_set(_glst,(int)_ixv->num,%s,%d); altair_val_free(_ixv); altair_val_free(%s); }",
                   n->idx_list,ie,val,n->line,val);
        } else {
            emitln(g,"{ AltairVal *_ixv=%s; altair_list_set(altair_var_get(%s_var),(int)_ixv->num,%s,%d); altair_val_free(_ixv); altair_val_free(%s); }",
                   ie,n->idx_list,val,n->line,val);
        }
        free(ie); free(val);
        break;
    }

    case ND_LOG: {
        if(n->nchildren>0){
            char *e=cg_expr(g,n->children[0]);
            emitln(g,"{ AltairVal *_lg%d=%s; altair_log(_lg%d); altair_val_free(_lg%d); }",
                   g->tmp,e,g->tmp,g->tmp); g->tmp++;
            free(e);
        }
        break;
    }

    case ND_IF: {
        int idx=0;
        char *cond=cg_expr(g,n->children[idx++]);
        int t=newtmp(g);
        emitln(g,"{ /* if */");
        g->indent++;
        emitln(g,"AltairVal *_if%d=%s;",t,cond); free(cond);
        emitln(g,"if(_if%d&&((_if%d->type==ALT_BOOL&&_if%d->boolean)||(_if%d->type==ALT_NUMERIC&&_if%d->num!=0))){",t,t,t,t,t);
        g->indent++;
        cg_block(g,n->children[idx++]);
        g->indent--;
        for(int e=0;e<n->nelif;e++){
            char *ec=cg_expr(g,n->children[idx++]);
            int te=newtmp(g);
            emitln(g,"} else { AltairVal *_elif%d=%s;",te,ec); free(ec);
            emitln(g,"if(_elif%d&&((_elif%d->type==ALT_BOOL&&_elif%d->boolean)||(_elif%d->type==ALT_NUMERIC&&_elif%d->num!=0))){",te,te,te,te,te);
            g->indent++;
            cg_block(g,n->children[idx++]);
            g->indent--;
        }
        if(n->has_else){
            emitln(g,"} else {");
            g->indent++;
            cg_block(g,n->children[idx]);
            g->indent--;
        }
        for(int e=0;e<=n->nelif;e++) emitln(g,"}");
        g->indent--;
        emitln(g,"}");
        break;
    }

    case ND_WHILE: {
        g->loop_depth++;
        int t=newtmp(g);
        emitln(g,"while(1){ /* while */");
        g->indent++;
        char *cond=cg_expr(g,n->count_expr);
        emitln(g,"AltairVal *_w%d=%s;",t,cond); free(cond);
        emitln(g,"if(!(_w%d&&((_w%d->type==ALT_BOOL&&_w%d->boolean)||(_w%d->type==ALT_NUMERIC&&_w%d->num!=0)))) break;",t,t,t,t,t);
        emitln(g,"altair_val_free(_w%d);",t);
        cg_block(g,n->body);
        g->indent--;
        emitln(g,"}");
        g->loop_depth--;
        break;
    }

    case ND_REPEAT: {
        g->loop_depth++;
        int t=newtmp(g);
        char *cnt=cg_expr(g,n->count_expr);
        emitln(g,"{ AltairVal *_r%d=%s; long long _ri%d=0;",t,cnt,t); free(cnt);
        g->indent++;
        emitln(g,"for(;_ri%d<(long long)_r%d->num;_ri%d++){",t,t,t);
        g->indent++;
        cg_block(g,n->body);
        g->indent--;
        emitln(g,"}");
        emitln(g,"altair_val_free(_r%d);",t);
        g->indent--;
        emitln(g,"}");
        g->loop_depth--;
        break;
    }

    case ND_FOREVER: {
        g->loop_depth++;
        emitln(g,"while(1){ /* forever */");
        g->indent++;
        cg_block(g,n->body);
        g->indent--;
        emitln(g,"}");
        g->loop_depth--;
        break;
    }

    case ND_FOREACH: {
        g->loop_depth++;
        int t=newtmp(g);
        int owns_fl=1;
        char *lst;
        if(n->iter_list_expr->kind==ND_IDENT){
            const char *name=n->iter_list_expr->str_val;
            if(g->in_method && is_field(g,name))
                lst=cg_fmt("altair_obj_get(_self,\"%s\",%d)",name,n->line);
            else if(is_token_var(g,name))
                lst=NULL;
            else if(g->in_fun && !is_local_var(g,name) && !is_catch_id(g,name))
                lst=cg_fmt("altair_var_get(altair_var_lookup(\"%s\"))",name);
            else
                lst=cg_fmt("altair_var_get(%s_var)",name);
            if(lst) owns_fl=0;
            else lst=cg_expr(g,n->iter_list_expr);
        } else {
            lst=cg_expr(g,n->iter_list_expr);
        }
        emitln(g,"{ AltairVal *_fl%d=%s;",t,lst); free(lst);
        g->indent++;
        emitln(g,"int _flen%d=altair_list_length(_fl%d);",t,t);
        emitln(g,"AltairVar *%s_var=altair_var_new(\"%s\",ALT_TEXT,ALT_RAM,0,0,0.0);",
               n->iter_var,n->iter_var);
        if(g->in_fun) push_local(g, n->iter_var);
        emitln(g,"for(int _fi%d=0;_fi%d<_flen%d;_fi%d++){",t,t,t,t);
        g->indent++;
        emitln(g,"altair_var_set(%s_var,altair_list_get(_fl%d,_fi%d,%d));",
               n->iter_var,t,t,n->line);
        cg_block(g,n->body);
        g->indent--;
        emitln(g,"}");

        emitln(g,"altair_var_release(&%s_var);",n->iter_var);
        if(owns_fl) emitln(g,"altair_val_free(_fl%d);",t);
        g->indent--;
        emitln(g,"}");
        g->loop_depth--;
        break;
    }

    case ND_EXIT: {
        if(g->loop_depth > 0)
            emitln(g,"break;");
        else
            emitln(g,"{ altair_shutdown(); exit(0); }");
        break;
    }

    case ND_WAIT: {
        emitln(g,"altair_wait(%g);", n->wait_secs);
        break;
    }

    case ND_RETURN: {
        if(n->nchildren>0){
            char *e=cg_expr(g,n->children[0]);
            emitln(g,"return %s;",e); free(e);
        } else {
            emitln(g,"return NULL;");
        }
        break;
    }

    case ND_MIGRATE: {
        if(n->migrate_by_name){
            if(g->in_fun && !is_local_var(g,n->migrate_var))
                emitln(g,"altair_migrate_name(altair_var_lookup(\"%s\"),\"%s\");",
                       n->migrate_var, n->migrate_state);
            else
                emitln(g,"altair_migrate_name(%s_var,\"%s\");",
                       n->migrate_var, n->migrate_state);
        } else {
            if(g->in_fun && !is_local_var(g,n->migrate_var))
                emitln(g,"altair_migrate(altair_var_lookup(\"%s\"),%d);",
                       n->migrate_var, n->migrate_num);
            else
                emitln(g,"altair_migrate(%s_var,%d);",
                       n->migrate_var, n->migrate_num);
        }
        break;
    }

    case ND_SNAPSHOT: {
        if(strcmp(n->snap_op,"create")==0)
            emitln(g,"altair_snapshot_create(\"%s\",%d);",n->snap_name,n->line);
        else if(strcmp(n->snap_op,"restore")==0)
            emitln(g,"altair_snapshot_restore(\"%s\",%d);",n->snap_name,n->line);
        else
            emitln(g,"altair_snapshot_delete(\"%s\",%d);",n->snap_name,n->line);
        break;
    }

    case ND_CHOOSE: {
        int t=newtmp(g);
        emitln(g,"AltairVar *%s_var=altair_var_new(\"%s\",ALT_TEXT,ALT_RAM,0,0,0.0);",
               n->choose_name,n->choose_name);
        if(g->in_fun) push_local(g, n->choose_name);
        emitln(g,"{ /* choose */");
        g->indent++;
        emit(g,"    double _cw%d[]={",t);
        for(int i=0;i<n->nchoose;i++) emit(g,"%g%s",n->choose_weights[i],i<n->nchoose-1?",":"");
        emit(g,"};\n");
        emit(g,"    const char *_co%d[]={",t);
        for(int i=0;i<n->nchoose;i++){
            char *esc=c_escape(n->choose_opts[i]);
            emit(g,"\"%s\"%s",esc,i<n->nchoose-1?",":""); free(esc);
        }
        emit(g,"};\n");
        emitln(g,"int _ci%d=altair_choose(_cw%d,%d);",t,t,n->nchoose);
        emitln(g,"altair_var_set_own(%s_var,altair_str(_co%d[_ci%d]));",n->choose_name,t,t);
        g->indent--;
        emitln(g,"}");
        break;
    }

    case ND_TRY_CATCH: {

        int t=newtmp(g);
        emitln(g,"{ /* try/catch */");
        g->indent++;
        emitln(g,"int _td%d = _altair_try_depth;  /* save BEFORE increment */",t);
        emitln(g,"if(_altair_try_depth < ALT_TRY_MAX) {");
        g->indent++;
        emitln(g,"_altair_err_stack[_altair_try_depth].active = 0;");
        emitln(g,"if(setjmp(_altair_jmp_stack[_altair_try_depth++]) == 0) {");
        g->indent++;
        cg_block(g,n->try_body);
        g->indent--;
        emitln(g,"}");

        emitln(g,"_altair_try_depth = _td%d; /* restaurar siempre, evita el crash en cadena */",t);
        g->indent--;
        emitln(g,"}");

        emitln(g,"AltairError _%s_err = _altair_err_stack[_td%d];",n->catch_ident,t);
        emitln(g,"if(_%s_err.active) {",n->catch_ident);
        g->indent++;
        emitln(g,"AltairVar *%s_code_var=altair_var_new(\"%s.code\",ALT_TEXT,ALT_RAM,0,0,0.0);",
               n->catch_ident,n->catch_ident);
        emitln(g,"altair_var_set_own(%s_code_var,altair_str(_%s_err.code));",n->catch_ident,n->catch_ident);
        emitln(g,"AltairVar *%s_message_var=altair_var_new(\"%s.message\",ALT_TEXT,ALT_RAM,0,0,0.0);",
               n->catch_ident,n->catch_ident);
        emitln(g,"altair_var_set_own(%s_message_var,altair_str(_%s_err.message));",n->catch_ident,n->catch_ident);
        emitln(g,"AltairVar *%s_line_var=altair_var_new(\"%s.line\",ALT_NUMERIC,ALT_RAM,0,0,0.0);",
               n->catch_ident,n->catch_ident);
        emitln(g,"altair_var_set_own(%s_line_var,altair_num(_%s_err.line));",n->catch_ident,n->catch_ident);
        if(g->ncatch_ids<16) strncpy(g->catch_ids[g->ncatch_ids++],n->catch_ident,63);
        if(g->in_fun){ push_local(g,n->catch_ident); }
        cg_block(g,n->catch_body);
        g->ncatch_ids--;

        emitln(g,"altair_var_release(&%s_code_var);",n->catch_ident);
        emitln(g,"altair_var_release(&%s_message_var);",n->catch_ident);
        emitln(g,"altair_var_release(&%s_line_var);",n->catch_ident);
        g->indent--;
        emitln(g,"}");
        g->indent--;
        emitln(g,"}");
        break;
    }

    case ND_RELEASE: {
        const char *rv=n->release_var;
        if(g->in_fun && !is_local_var(g,rv)){
            emitln(g,"{ AltairVar *_rv%d=altair_var_lookup(\"%s\"); altair_var_release(&_rv%d); }",
                   g->tmp,rv,g->tmp); g->tmp++;
        } else {
            emitln(g,"altair_var_release(&%s_var);",rv);
        }
        break;
    }

    case ND_CALL_GALAXY: {
        emitln(g,"/* call galaxy: %s - not yet supported in this build */",n->galaxy_name);
        break;
    }

    case ND_EXPR_STMT: {
        if(n->nchildren>0){
            ASTNode *inner=n->children[0];

            if(inner->kind==ND_IDENT && is_known_fun(g,inner->str_val) &&
               !(g->in_method && is_field(g,inner->str_val)) &&
               !is_local_var(g,inner->str_val)){
                emitln(g,"{ AltairVal *_es%d=_fn_%s(); altair_val_free(_es%d); }",
                       g->tmp,inner->str_val,g->tmp); g->tmp++;
                break;
            }
            char *e=cg_expr(g,n->children[0]);
            emitln(g,"{ AltairVal *_es%d=%s; altair_val_free(_es%d); }",g->tmp,e,g->tmp); g->tmp++;
            free(e);
        }
        break;
    }

    case ND_LISTEN: {
        emitln(g,"altair_server_init(%d);",n->server_port);

        cg_block(g,n->body);

        emitln(g,"altair_server_run();");
        break;
    }

    case ND_ROUTE: {

        char hname[200];
        snprintf(hname, sizeof(hname), "_route_%s_%s_handler",
                 n->route_method, n->handler_name[0]?n->handler_name:"anon");

        emitln(g,"/* route handler for %s %s */",n->route_method,n->route_path);
        emitln(g,"altair_server_add_route(\"%s\",\"%s\",%s,%d);",
               n->route_method,n->route_path,hname,n->rate_limit);
        break;
    }

    case ND_MIDDLEWARE: {
        char hname[200];
        snprintf(hname,sizeof(hname),"_middleware_%s",n->handler_name);
        emitln(g,"altair_server_add_middleware(%s);",hname);
        break;
    }

    case ND_JOB: {
        char hname[200];
        snprintf(hname,sizeof(hname),"_job_%s",n->handler_name);
        emitln(g,"altair_job_register(\"%s\",%s,%ld);",
               n->handler_name,hname,(long)n->job_interval_secs);
        break;
    }

    case ND_HEALTH: {
        emitln(g,"altair_server_add_route(\"GET\",\"%s\",altair_health_handler,0);",n->health_path);

        for(int i=0;i<n->nchildren;i++) cg_stmt(g,n->children[i]);
        break;
    }

    case ND_METRICS: {
        emitln(g,"altair_server_add_route(\"GET\",\"%s\",altair_metrics_handler,0);",n->metrics_path);
        break;
    }

    case ND_ON_SHUTDOWN: {
        char hname[200];
        snprintf(hname,sizeof(hname),"_on_shutdown_%d",newtmp(g));
        emitln(g,"altair_server_set_shutdown(%s);",hname);
        break;
    }

    case ND_SESSION_DECL: {
        emitln(g,"/* session %s - use altair_session_get/set at runtime */",n->session_var);
        emitln(g,"AltairVar *%s_var = altair_var_new(\"%s\",ALT_TEXT,ALT_RAM,0,0,%.1f);",
               n->session_var,n->session_var,n->session_ttl_secs);
        if(g->in_fun) push_local(g, n->session_var);
        break;
    }

    case ND_CONFIG_DECL: {

        cg_block(g,n->body);
        break;
    }

    case ND_DB_POOL: {
        char *esc=c_escape(n->db_url);
        emitln(g,"/* db_pool %s: connect to %s max %d */",n->db_var,esc,n->db_max);
        emitln(g,"/* NOTE: install your DB client library and call its connect() here */");
        emitln(g,"AltairVar *%s_var = altair_var_new(\"%s\",ALT_TEXT,ALT_RAM,0,0,0.0);",
               n->db_var,n->db_var);
        emitln(g,"altair_var_set_own(%s_var,altair_str(\"%s\"));",n->db_var,esc);
        if(g->in_fun) push_local(g, n->db_var);
        free(esc);
        break;
    }

    case ND_FUN_DECL:
    case ND_CLASS_DECL:
        break;

    case ND_BLOCK:
        cg_block(g,n);
        break;

    case ND_LINK:

        if(strcmp(n->gfx_link_lib,"raylib")==0) g->use_raylib=1;
        break;

    case ND_WINDOW_DECL: {

        if(!g->use_raylib){ emitln(g,"/* window: requires 'link graphics raylib' */"); break; }
        ASTNode *pTitle  = gfx_prop(n,"title");
        ASTNode *pWidth  = gfx_prop(n,"width");
        ASTNode *pHeight = gfx_prop(n,"height");
        ASTNode *pFps    = gfx_prop(n,"fps");
        char *W = cg_rl_int(g, pWidth  ? pWidth  : NULL);
        char *H = cg_rl_int(g, pHeight ? pHeight : NULL);
        char *T = cg_rl_str(g, pTitle  ? pTitle  : NULL);
        char *F = cg_rl_int(g, pFps    ? pFps    : NULL);
        if(!pWidth)  { free(W); W=strdup("800"); }
        if(!pHeight) { free(H); H=strdup("600"); }
        if(!pTitle)  { free(T); T=strdup("\"Altair App\""); }
        if(!pFps)    { free(F); F=strdup("60"); }
        emitln(g,"InitWindow(%s, %s, %s);",W,H,T);
        emitln(g,"SetTargetFPS(%s);",F);
        free(W); free(H); free(T); free(F);
        break;
    }

    case ND_LOOP: {

        if(!g->use_raylib){

            g->loop_depth++;
            emitln(g,"while(1) {");
            g->indent++;
            cg_block(g,n->body);
            g->indent--;
            emitln(g,"}");
            g->loop_depth--;
            break;
        }
        g->loop_depth++;
        emitln(g,"while(!WindowShouldClose()) {");
        g->indent++;
        emitln(g,"BeginDrawing();");
        cg_block(g,n->body);
        emitln(g,"EndDrawing();");
        g->indent--;
        emitln(g,"}");
        g->loop_depth--;
        break;
    }

    case ND_DRAW_CMD: {
        if(!g->use_raylib){ emitln(g,"/* draw: requires raylib */"); break; }
        const char *kind = n->gfx_kind[0] ? n->gfx_kind : "rect";
        ASTNode *px = gfx_prop(n,"x");
        ASTNode *py = gfx_prop(n,"y");
        ASTNode *pw = gfx_prop(n,"width")  ? gfx_prop(n,"width")  : gfx_prop(n,"w");
        ASTNode *ph = gfx_prop(n,"height") ? gfx_prop(n,"height") : gfx_prop(n,"h");
        ASTNode *pr = gfx_prop(n,"radius") ? gfx_prop(n,"radius") : gfx_prop(n,"r");
        ASTNode *pcolor = gfx_prop(n,"color");
        ASTNode *ptext  = gfx_prop(n,"text") ? gfx_prop(n,"text") : gfx_prop(n,"content");
        ASTNode *psize  = gfx_prop(n,"size") ? gfx_prop(n,"size") : gfx_prop(n,"font_size");
        ASTNode *pimg   = gfx_prop(n,"image") ? gfx_prop(n,"image") : gfx_prop(n,"src");
        ASTNode *px2    = gfx_prop(n,"x2");
        ASTNode *py2    = gfx_prop(n,"y2");
        ASTNode *pthick = gfx_prop(n,"thick") ? gfx_prop(n,"thick") : gfx_prop(n,"thickness");

        char *X  = cg_rl_int(g, px);
        char *Y  = cg_rl_int(g, py);
        char *W  = pw ? cg_rl_int(g, pw) : strdup("100");
        char *H2 = ph ? cg_rl_int(g, ph) : strdup("100");
        char *R  = pr ? cg_rl_float(g, pr) : strdup("50.0f");
        char *C  = cg_rl_color_expr(g, pcolor);
        char *SZ = psize ? cg_rl_int(g, psize) : strdup("20");
        char *TXT= ptext ? cg_rl_str(g, ptext)  : strdup("\"\"");
        char *X2 = px2  ? cg_rl_int(g, px2)     : strdup("0");
        char *Y2 = py2  ? cg_rl_int(g, py2)     : strdup("0");
        char *TH = pthick ? cg_rl_int(g, pthick) : strdup("1");

        if(strcmp(kind,"text")==0){
            emitln(g,"DrawText(%s, %s, %s, %s, %s);",TXT,X,Y,SZ,C);
        } else if(strcmp(kind,"rect")==0||strcmp(kind,"rectangle")==0){
            emitln(g,"DrawRectangle(%s, %s, %s, %s, %s);",X,Y,W,H2,C);
        } else if(strcmp(kind,"circle")==0){
            emitln(g,"DrawCircle(%s, %s, %s, %s);",X,Y,R,C);
        } else if(strcmp(kind,"line")==0){
            emitln(g,"DrawLine(%s, %s, %s, %s, %s);",X,Y,X2,Y2,C);
        } else if(strcmp(kind,"line_thick")==0||strcmp(kind,"thick_line")==0){
            emitln(g,"DrawLineEx((Vector2){%s,%s},(Vector2){%s,%s},%s,%s);",X,Y,X2,Y2,TH,C);
        } else if(strcmp(kind,"image")==0||strcmp(kind,"texture")==0){

            if(pimg && pimg->kind==ND_IDENT)
                emitln(g,"DrawTexture(%s, %s, %s, %s);",pimg->str_val,X,Y,C);
            else
                emitln(g,"/* draw image: 'image' property must be a texture variable */");
        } else if(strcmp(kind,"pixel")==0){
            emitln(g,"DrawPixel(%s, %s, %s);",X,Y,C);
        } else if(strcmp(kind,"triangle")==0){
            char *X3=gfx_prop(n,"x3")?cg_rl_int(g,gfx_prop(n,"x3")):strdup("0");
            char *Y3=gfx_prop(n,"y3")?cg_rl_int(g,gfx_prop(n,"y3")):strdup("0");
            emitln(g,"DrawTriangle((Vector2){%s,%s},(Vector2){%s,%s},(Vector2){%s,%s},%s);",
                   X,Y,X2,Y2,X3,Y3,C);
            free(X3); free(Y3);
        } else {
            emitln(g,"DrawRectangle(%s, %s, %s, %s, %s); /* draw %s */",X,Y,W,H2,C,kind);
        }
        free(X); free(Y); free(W); free(H2); free(R); free(C);
        free(SZ); free(TXT); free(X2); free(Y2); free(TH);
        break;
    }

    case ND_CLEAR_STMT: {
        if(!g->use_raylib){ emitln(g,"/* clear: requires raylib */"); break; }
        if(n->left){
            char *c=cg_rl_color_expr(g,n->left);
            emitln(g,"ClearBackground(%s);",c); free(c);
        } else {
            const char *c=rl_color(n->gfx_kind);
            emitln(g,"ClearBackground(%s);",c?c:"RAYWHITE");
        }
        break;
    }

    case ND_COLOR_DECL: {

        if(!g->use_raylib){ emitln(g,"/* color: requires raylib */"); break; }
        if(n->nchildren>0){
            char *c=cg_rl_color_expr(g,n->children[0]);
            emitln(g,"Color %s = %s;",n->var_name,c); free(c);
        } else {
            emitln(g,"Color %s = WHITE;",n->var_name);
        }
        break;
    }

    case ND_IMAGE_DECL: {
        if(!g->use_raylib){ emitln(g,"/* image: requires raylib */"); break; }
        char *esc=c_escape(n->str_val);
        emitln(g,"Texture2D %s = LoadTexture(\"%s\");",n->var_name,esc);
        free(esc);
        break;
    }

    case ND_SOUND_DECL: {
        if(!g->use_raylib){ emitln(g,"/* sound: requires raylib */"); break; }
        char *esc=c_escape(n->str_val);
        emitln(g,"Sound %s = LoadSound(\"%s\");",n->var_name,esc);
        free(esc);
        break;
    }

    case ND_MUSIC_DECL: {
        if(!g->use_raylib){ emitln(g,"/* music: requires raylib */"); break; }
        char *esc=c_escape(n->str_val);
        emitln(g,"Music %s = LoadMusicStream(\"%s\");",n->var_name,esc);
        free(esc);
        break;
    }

    case ND_PLAY_STMT: {
        if(!g->use_raylib){ emitln(g,"/* play: requires raylib */"); break; }
        emitln(g,"/* play %s - call PlaySound(%s) or PlayMusicStream(%s) */",
               n->var_name, n->var_name, n->var_name);
        emitln(g,"PlaySound(%s);",n->var_name);
        break;
    }

    case ND_STOP_STMT: {
        if(!g->use_raylib){ emitln(g,"/* stop: requires raylib */"); break; }
        emitln(g,"StopSound(%s);",n->var_name);
        break;
    }

    case ND_PAUSE_STMT: {
        if(!g->use_raylib){ emitln(g,"/* pause: requires raylib */"); break; }
        emitln(g,"PauseSound(%s);",n->var_name);
        break;
    }

    case ND_TIMER_DECL: {

        if(n->nchildren>0){
            char *v=cg_rl_float(g,n->children[0]);
            emitln(g,"float %s = %s;",n->var_name,v); free(v);
        } else {
            emitln(g,"float %s = 0.0f;",n->var_name);
        }
        break;
    }

    case ND_WIDGET_DECL: {

        if(!g->use_raylib){ emitln(g,"/* widget: requires raylib */"); break; }
        ASTNode *px2=gfx_prop(n,"x"), *py2=gfx_prop(n,"y");
        ASTNode *pw2=gfx_prop(n,"width")?gfx_prop(n,"width"):gfx_prop(n,"w");
        ASTNode *ph2=gfx_prop(n,"height")?gfx_prop(n,"height"):gfx_prop(n,"h");
        char *X=cg_rl_int(g,px2), *Y=cg_rl_int(g,py2);
        char *W2=pw2?cg_rl_int(g,pw2):strdup("120");
        char *H3=ph2?cg_rl_int(g,ph2):strdup("40");
        if(n->var_name[0]){
            emitln(g,"Rectangle %s_bounds = {%s,%s,%s,%s}; /* %s widget */",
                   n->var_name,X,Y,W2,H3,n->gfx_kind);
            emitln(g,"bool %s_hover = CheckCollisionPointRec(GetMousePosition(),%s_bounds);",
                   n->var_name,n->var_name);
            emitln(g,"bool %s_clicked = %s_hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);",
                   n->var_name,n->var_name);
        }

        ASTNode *pc=gfx_prop(n,"color");
        char *C=cg_rl_color_expr(g,pc);
        emitln(g,"DrawRectangleRec(%s_bounds, %s);",n->var_name,C);
        ASTNode *ptxt=gfx_prop(n,"text")?gfx_prop(n,"text"):gfx_prop(n,"label");
        if(ptxt){
            char *T=cg_rl_str(g,ptxt);
            emitln(g,"DrawText(%s, %s+4, %s+4, 16, BLACK);",T,X,Y); free(T);
        }
        free(X); free(Y); free(W2); free(H3); free(C);
        break;
    }

    case ND_MENU_DECL: {
        emitln(g,"/* menu %s - define items as widget buttons */",n->var_name);
        break;
    }

    case ND_DIALOG_DECL: {
        emitln(g,"/* dialog %s */",n->var_name);
        cg_block(g,n->body);
        break;
    }

    case ND_SCENE_DECL: {

        emitln(g,"_scene_%s:;",n->var_name);
        g->gfx_scene_count++;
        break;
    }

    case ND_GOTO_STMT: {
        emitln(g,"goto _scene_%s;",n->var_name);
        break;
    }

    case ND_CURSOR_STMT: {
        if(!g->use_raylib){ break; }

        const char *kind=n->gfx_kind[0]?n->gfx_kind:"default";
        if(strcmp(kind,"hidden")==0)     emitln(g,"HideCursor();");
        else if(strcmp(kind,"show")==0)  emitln(g,"ShowCursor();");
        else if(strcmp(kind,"default")==0) emitln(g,"SetMouseCursor(MOUSE_CURSOR_DEFAULT);");
        else if(strcmp(kind,"crosshair")==0) emitln(g,"SetMouseCursor(MOUSE_CURSOR_CROSSHAIR);");
        else if(strcmp(kind,"hand")==0)  emitln(g,"SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);");
        else if(strcmp(kind,"resize")==0)emitln(g,"SetMouseCursor(MOUSE_CURSOR_RESIZE_ALL);");
        else emitln(g,"SetMouseCursor(MOUSE_CURSOR_DEFAULT);");
        break;
    }

    case ND_ANIMATE_DECL: {

        ASTNode *pfrom = gfx_prop(n,"from");
        ASTNode *pto   = gfx_prop(n,"to");
        ASTNode *pdur  = gfx_prop(n,"duration");
        char *F = pfrom ? cg_rl_float(g,pfrom) : strdup("0.0f");
        char *T = pto   ? cg_rl_float(g,pto)   : strdup("1.0f");
        char *D = pdur  ? cg_rl_float(g,pdur)  : strdup("1.0f");
        int t=newtmp(g);
        emitln(g,"static float _anim_t%d = 0.0f;",t);
        emitln(g,"_anim_t%d += GetFrameTime() / %s;",t,D);
        emitln(g,"if(_anim_t%d > 1.0f) _anim_t%d = 1.0f;",t,t);
        if(n->var_name[0] && n->gfx_kind[0]){
            emitln(g,"%s.%s = %s + (_anim_t%d * (%s - %s));",
                   n->var_name, n->gfx_kind, F, t, T, F);
        }
        free(F); free(T); free(D);
        break;
    }

    case ND_POPUP_DECL: {
        emitln(g,"/* popup %s */",n->var_name);
        cg_block(g,n->body);
        break;
    }

    case ND_CANVAS_GFX: {
        emitln(g,"/* canvas %s */",n->var_name);
        cg_block(g,n->body);
        break;
    }

    case ND_LAYOUT: {

        emitln(g,"{ /* layout: %s */",n->gfx_kind);
        g->indent++;
        cg_block(g,n->body);
        g->indent--;
        emitln(g,"}");
        break;
    }

    default:
        break;
    }
}

static void cg_route_handler(CG *g, ASTNode *n){
    char hname[200];
    snprintf(hname,sizeof(hname),"_route_%s_%s_handler",
             n->route_method, n->handler_name[0]?n->handler_name:"anon");
    fprintf(g->fp,"static void %s(AltairRequest *_altair_req_arg, AltairResponse *_altair_res_arg) {\n",hname);
    fprintf(g->fp,"    _altair_req = _altair_req_arg; _altair_res = _altair_res_arg;\n");
    g->indent=1;
    int saved_in_fun=g->in_fun; g->in_fun=1;
    int saved_nlocal=g->nlocal; g->nlocal=0;
    cg_block(g,n->body);
    g->in_fun=saved_in_fun; g->nlocal=saved_nlocal;
    g->indent=0;
    fprintf(g->fp,"}\n\n");
}

static void cg_middleware_handler(CG *g, ASTNode *n){
    char hname[200];
    snprintf(hname,sizeof(hname),"_middleware_%s",n->handler_name);
    fprintf(g->fp,"static int %s(AltairRequest *_altair_req_arg, AltairResponse *_altair_res_arg) {\n",hname);
    fprintf(g->fp,"    _altair_req = _altair_req_arg; _altair_res = _altair_res_arg;\n");
    g->indent=1;
    int saved_in_fun=g->in_fun; g->in_fun=1;
    int saved_nlocal=g->nlocal; g->nlocal=0;
    cg_block(g,n->body);
    g->in_fun=saved_in_fun; g->nlocal=saved_nlocal;
    g->indent=0;
    fprintf(g->fp,"    return 1; /* continue chain */\n}\n\n");
}

static void cg_job_handler(CG *g, ASTNode *n){
    char hname[200];
    snprintf(hname,sizeof(hname),"_job_%s",n->handler_name);
    fprintf(g->fp,"static void %s(void) {\n",hname);
    g->indent=1;
    int saved_in_fun=g->in_fun; g->in_fun=1;
    int saved_nlocal=g->nlocal; g->nlocal=0;
    cg_block(g,n->body);
    g->in_fun=saved_in_fun; g->nlocal=saved_nlocal;
    g->indent=0;
    fprintf(g->fp,"}\n\n");
}

static void cg_shutdown_handler(CG *g, ASTNode *n){
    int t=newtmp(g);
    char hname[64]; snprintf(hname,sizeof(hname),"_on_shutdown_%d",t);
    fprintf(g->fp,"static void %s(void) {\n",hname);
    g->indent=1;
    int saved_in_fun=g->in_fun; g->in_fun=1;
    int saved_nlocal=g->nlocal; g->nlocal=0;
    cg_block(g,n->body);
    g->in_fun=saved_in_fun; g->nlocal=saved_nlocal;
    g->indent=0;
    fprintf(g->fp,"}\n\n");
}

static void cg_class(CG *g, ASTNode *n){
    const char *cname=n->class_name;
    emitln(g,"/* ===== class %s ===== */",cname);

    char  fnames[32][128]; VType ftypes[32]; StorKind fstors[32];
    ASTNode *finits[32];
    int   nf=0;
    ASTNode *methods[32]; int nm=0;

    for(int i=0;i<n->nchildren;i++){
        ASTNode *m=n->children[i];
        if(!m) continue;
        if(m->kind==ND_VAR_DECL){
            strncpy(fnames[nf],m->var_name,127);
            ftypes[nf]=m->var_type;
            fstors[nf]=m->storage==STOR_ORBIT||m->storage==STOR_PREFER ?
                        STOR_RAM : m->storage;
            finits[nf]=(m->nchildren>0 && m->children[0]) ? m->children[0] : NULL;
            nf++;
        } else if(m->kind==ND_FUN_DECL && nm<32) {
            methods[nm++]=m;
        }
    }

    fprintf(g->fp,"AltairObj *_class_%s_new(void) {\n",cname);
    fprintf(g->fp,"    const char *_fn[]={");
    for(int i=0;i<nf;i++) fprintf(g->fp,"\"%s\"%s",fnames[i],i<nf-1?",":"");
    fprintf(g->fp,"};\n");
    fprintf(g->fp,"    AltairVal *_fd[%d];\n",nf>0?nf:1);
    for(int i=0;i<nf;i++){
        if(finits[i]){
            char *e=cg_expr(g,finits[i]);
            fprintf(g->fp,"    _fd[%d]=%s;\n",i,e);
            free(e);
        } else {
            switch(ftypes[i]){
            case VTYPE_TEXT:    fprintf(g->fp,"    _fd[%d]=altair_str(\"\");\n",i); break;
            case VTYPE_BOOL:    fprintf(g->fp,"    _fd[%d]=altair_bool(0);\n",i); break;
            case VTYPE_LIST:    fprintf(g->fp,"    _fd[%d]=altair_list_new();\n",i); break;
            default:            fprintf(g->fp,"    _fd[%d]=altair_num(0.0);\n",i); break;
            }
        }
    }
    fprintf(g->fp,"    AltStorage _fs[%d];\n",nf>0?nf:1);
    for(int i=0;i<nf;i++) fprintf(g->fp,"    _fs[%d]=%s;\n",i,stor_c(fstors[i]));
    fprintf(g->fp,"    AltairObj *_result=altair_obj_new(\"%s\",%d,_fn,_fd,_fs);\n",cname,nf);
    for(int i=0;i<nf;i++) fprintf(g->fp,"    altair_val_free(_fd[%d]);\n",i);
    fprintf(g->fp,"    return _result;\n");
    fprintf(g->fp,"}\n\n");

    char saved_class[128]; strncpy(saved_class,g->cur_class,127);
    int saved_nf=g->nclass_fields, saved_im=g->in_method;
    strncpy(g->cur_class,cname,127);
    g->nclass_fields=nf;
    for(int i=0;i<nf;i++) strncpy(g->class_fields[i],fnames[i],127);
    g->in_method=1;

    for(int mi=0;mi<nm;mi++){
        ASTNode *mth=methods[mi];
        fprintf(g->fp,"AltairVal *_class_%s_method_%s(AltairObj *_self",cname,mth->fun_name);
        for(int pi=0;pi<mth->nparam;pi++)
            fprintf(g->fp,",AltairVal *_p_%s",mth->param_names[pi]);
        fprintf(g->fp,"){\n");
        g->indent=1;
        int saved_in_fun=g->in_fun; g->in_fun=1;
        int saved_nlocal=g->nlocal; g->nlocal=0;

        for(int pi=0;pi<mth->nparam;pi++){
            emitln(g,"AltairVar *%s_var=altair_var_new(\"%s\",%s,ALT_RAM,0,0,0.0);",
                   mth->param_names[pi],mth->param_names[pi],vtype_c(mth->param_types[pi]));
            emitln(g,"altair_var_set(%s_var,_p_%s);",mth->param_names[pi],mth->param_names[pi]);
            push_local(g, mth->param_names[pi]);
        }
        cg_block(g,mth->fun_body);

        for(int pi=0;pi<mth->nparam;pi++)
            emitln(g,"altair_var_release(&%s_var);",mth->param_names[pi]);
        emitln(g,"return NULL;");
        g->in_fun=saved_in_fun; g->nlocal=saved_nlocal;
        g->indent=0;
        fprintf(g->fp,"}\n\n");
    }

    strncpy(g->cur_class,saved_class,127);
    g->nclass_fields=saved_nf;
    g->in_method=saved_im;
}

static void cg_fun(CG *g, ASTNode *n){
    fprintf(g->fp,"/* function %s */\n",n->fun_name);

    fprintf(g->fp,"AltairVal *");
    fprintf(g->fp," _fn_%s(",n->fun_name);
    for(int i=0;i<n->nparam;i++){
        if(i>0) fprintf(g->fp,",");
        fprintf(g->fp,"AltairVal *_p_%s",n->param_names[i]);
    }
    fprintf(g->fp,"){\n");
    g->indent=1;
    int saved_in_fun=g->in_fun; g->in_fun=1;
    int saved_nlocal=g->nlocal; g->nlocal=0;

    for(int i=0;i<n->nparam;i++){
        emitln(g,"AltairVar *%s_var=altair_var_new(\"%s\",%s,ALT_RAM,0,0,0.0);",
               n->param_names[i],n->param_names[i],vtype_c(n->param_types[i]));
        emitln(g,"altair_var_set(%s_var,_p_%s);",n->param_names[i],n->param_names[i]);
        push_local(g, n->param_names[i]);
    }
    cg_block(g,n->fun_body);

    for(int i=0;i<n->nparam;i++)
        emitln(g,"altair_var_release(&%s_var);",n->param_names[i]);
    emitln(g,"return altair_num(0.0);");
    g->in_fun=saved_in_fun; g->nlocal=saved_nlocal;
    g->indent=0;
    fprintf(g->fp,"}\n\n");
}

void codegen_emit(ASTNode *program, FILE *fp,
                  const char *runtime_h_path, const char *runtime_c_path,
                  const char *source_file){
    CG g={0}; g.fp=fp; g.indent=0; g.tmp=0; g.source_file=source_file;

    ASTNode *body_pre=NULL;
    for(int i=0;i<program->nchildren;i++){
        if(program->children[i]&&program->children[i]->kind==ND_BLOCK)
            body_pre=program->children[i];
    }
    int has_audio=0;
    if(body_pre){
        for(int i=0;i<body_pre->nchildren;i++){
            ASTNode *s=body_pre->children[i];
            if(!s) continue;
            if(s->kind==ND_LINK && strcmp(s->gfx_link_lib,"raylib")==0)
                g.use_raylib=1;
            if(s->kind==ND_SOUND_DECL||s->kind==ND_MUSIC_DECL)
                has_audio=1;
        }
    }

    fprintf(fp,"/* ===== Altair Runtime (embebido) ===== */\n");
    fprintf(fp,"#define _POSIX_C_SOURCE 200809L\n");
    fprintf(fp,"#ifndef _WIN32\n");
    fprintf(fp,"#include <pthread.h>\n");
    fprintf(fp,"#endif\n");

    FILE *rh=fopen(runtime_h_path,"r");
    if(rh){ char buf[4096]; while(fgets(buf,sizeof(buf),rh)) fputs(buf,fp); fclose(rh); }

    FILE *rc=fopen(runtime_c_path,"r");
    if(rc){
        char buf[4096];
        while(fgets(buf,sizeof(buf),rc)){
            if(strstr(buf,"#include \"altair_rt.h\"")) continue;
            fputs(buf,fp);
        }
        fclose(rc);
    }
    fprintf(fp,"\n/* ===== End of Runtime ===== */\n\n");

    if(g.use_raylib){
        fprintf(fp,"/* ===== Raylib (v1.7.5vB graphics) ===== */\n");
        fprintf(fp,"#include \"raylib.h\"\n\n");
    }

    ASTNode *body=NULL;
    char hdr_name[128]="altair", hdr_version[64]="1.0", hdr_author[128]="";

    for(int i=0;i<program->nchildren;i++){
        ASTNode *c=program->children[i];
        if(!c) continue;
        if(c->kind==ND_HEADER){
            for(int j=0;j<c->nchildren;j++){
                ASTNode *kv=c->children[j];
                if(!kv||!kv->right) continue;
                if(strcmp(kv->var_name,"name")==0)    strncpy(hdr_name,kv->right->str_val,127);
                if(strcmp(kv->var_name,"version")==0) strncpy(hdr_version,kv->right->str_val,63);
                if(strcmp(kv->var_name,"author")==0)  strncpy(hdr_author,kv->right->str_val,127);
            }
        }
        if(c->kind==ND_BLOCK) body=c;
    }

    if(body){
        for(int i=0;i<body->nchildren;i++){
            ASTNode *s=body->children[i];
            if(!s) continue;
            if(s->kind==ND_CLASS_DECL && g.nknown<64)
                strncpy(g.known_classes[g.nknown++],s->class_name,127);
            if(s->kind==ND_FUN_DECL && g.nknown_funs<128)
                strncpy(g.known_funs[g.nknown_funs++],s->fun_name,127);
        }
    }

    fprintf(fp,"/* ===== Forward declarations ===== */\n");
    if(body){
        for(int i=0;i<body->nchildren;i++){
            ASTNode *s=body->children[i];
            if(!s) continue;
            if(s->kind==ND_CLASS_DECL){
                fprintf(fp,"AltairObj *_class_%s_new(void);\n",s->class_name);
                for(int j=0;j<s->nchildren;j++){
                    ASTNode *m=s->children[j];
                    if(!m||m->kind!=ND_FUN_DECL) continue;
                    fprintf(fp,"AltairVal *_class_%s_method_%s(AltairObj *_self",
                            s->class_name,m->fun_name);
                    for(int pi=0;pi<m->nparam;pi++)
                        fprintf(fp,",AltairVal *_p_%s",m->param_names[pi]);
                    fprintf(fp,");\n");
                }
            }
            if(s->kind==ND_FUN_DECL){
                fprintf(fp,"AltairVal *");
                fprintf(fp," _fn_%s(",s->fun_name);
                for(int pi=0;pi<s->nparam;pi++){
                    if(pi>0) fprintf(fp,",");
                    fprintf(fp,"AltairVal *_p_%s",s->param_names[pi]);
                }
                fprintf(fp,");\n");
            }

            if(s->kind==ND_ROUTE){
                char hname[200];
                snprintf(hname,sizeof(hname),"_route_%s_%s_handler",
                         s->route_method, s->handler_name[0]?s->handler_name:"anon");
                fprintf(fp,"static void %s(AltairRequest *, AltairResponse *);\n",hname);
            }
            if(s->kind==ND_MIDDLEWARE){
                fprintf(fp,"static int _middleware_%s(AltairRequest *, AltairResponse *);\n",s->handler_name);
            }
            if(s->kind==ND_JOB){
                fprintf(fp,"static void _job_%s(void);\n",s->handler_name);
            }
        }

        for(int i=0;i<body->nchildren;i++){
            ASTNode *s=body->children[i];
            if(s&&s->kind==ND_ON_SHUTDOWN){
                fprintf(fp,"static void _on_shutdown_%d(void);\n",g.tmp);
            }
        }
    }
    fprintf(fp,"\n");

    fprintf(fp,"static AltairVal *altair_val_from_obj(AltairObj *o){\n");
    fprintf(fp,"    AltairVal *v=(AltairVal*)calloc(1,sizeof(AltairVal));\n");
    fprintf(fp,"    v->type=ALT_OBJECT; v->obj=o; return v;\n");
    fprintf(fp,"}\n\n");

    fprintf(fp,"/* ===== Classes ===== */\n");
    if(body){
        for(int i=0;i<body->nchildren;i++){
            ASTNode *s=body->children[i];
            if(s&&s->kind==ND_CLASS_DECL) cg_class(&g,s);
        }
    }

    fprintf(fp,"\n/* ===== Functions ===== */\n");
    if(body){
        for(int i=0;i<body->nchildren;i++){
            ASTNode *s=body->children[i];
            if(s&&s->kind==ND_FUN_DECL) cg_fun(&g,s);
        }
    }

    fprintf(fp,"\n/* ===== Server handlers ===== */\n");
    if(body){
        for(int i=0;i<body->nchildren;i++){
            ASTNode *s=body->children[i];
            if(!s) continue;
            if(s->kind==ND_ROUTE)      cg_route_handler(&g,s);
            if(s->kind==ND_MIDDLEWARE) cg_middleware_handler(&g,s);
            if(s->kind==ND_JOB)        cg_job_handler(&g,s);
            if(s->kind==ND_ON_SHUTDOWN)cg_shutdown_handler(&g,s);
        }
    }

    fprintf(fp,"\n/* ===== main ===== */\n");
    fprintf(fp,"int main(int argc,char **argv){\n");
    g.indent=1;
    char *esc_name=c_escape(hdr_name);
    char *esc_ver=c_escape(hdr_version);
    char *esc_auth=c_escape(hdr_author);
    emitln(&g,"altair_init(\"%s\",\"%s\",\"%s\");",esc_name,esc_ver,esc_auth);
    free(esc_name); free(esc_ver); free(esc_auth);
    emitln(&g,"altair_set_args(argc,argv);");

    if(g.use_raylib && has_audio)
        emitln(&g,"InitAudioDevice();");

    if(body){
        for(int i=0;i<body->nchildren;i++){
            ASTNode *s=body->children[i];
            if(!s) continue;
            if(s->kind==ND_CLASS_DECL||s->kind==ND_FUN_DECL||
               s->kind==ND_ROUTE||s->kind==ND_MIDDLEWARE||
               s->kind==ND_JOB||s->kind==ND_ON_SHUTDOWN||
               s->kind==ND_LINK ) continue;
            cg_stmt(&g,s);
        }
    }

    if(g.use_raylib && has_audio) emitln(&g,"CloseAudioDevice();");
    if(g.use_raylib) emitln(&g,"CloseWindow();");

    emitln(&g,"altair_shutdown();");
#ifdef _WIN32

    emitln(&g,"altair_pause_if_own_console();");
#endif
    emitln(&g,"return 0;");
    fprintf(fp,"}\n");
}

