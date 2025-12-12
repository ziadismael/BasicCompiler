#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace std;

/*
{ Sample program
  in TINY language
  compute factorial
}

read x; {input an integer}
if 0<x then {compute only if x>=1}
  fact:=1;
  repeat
    fact := fact * x; 
    x:=x-1
  until x=0;
  write fact {output factorial}
end
*/

// sequence of statements separated by ;
// no procedures - no declarations
// all variables are integers
// variables are declared simply by assigning values to them :=
// if-statement: if (boolean) then [else] end
// repeat-statement: repeat until (boolean)
// boolean only in if and repeat conditions < = and two mathematical expressions
// math expressions integers only, + - * / ^
// I/O read write
// Comments {}

////////////////////////////////////////////////////////////////////////////////////
// Strings /////////////////////////////////////////////////////////////////////////

bool Equals(const char* a, const char* b)
{
    return strcmp(a, b)==0;
}

bool StartsWith(const char* a, const char* b)
{
    int nb=strlen(b);
    return strncmp(a, b, nb)==0;
}

void Copy(char* a, const char* b, int n=0)
{
    if(n>0) {strncpy(a, b, n); a[n]=0;}
    else strcpy(a, b);
}

void AllocateAndCopy(char** a, const char* b)
{
    if(b==0) {*a=0; return;}
    int n=strlen(b);
    *a=new char[n+1];
    strcpy(*a, b);
}

////////////////////////////////////////////////////////////////////////////////////
// Input and Output ////////////////////////////////////////////////////////////////

#define MAX_LINE_LENGTH 10000

struct InFile
{
    FILE* file;
    int cur_line_num;

    char line_buf[MAX_LINE_LENGTH];
    int cur_ind, cur_line_size;

    InFile(const char* str) {file=0; if(str) file=fopen(str, "r"); cur_line_size=0; cur_ind=0; cur_line_num=0;}
    ~InFile(){if(file) fclose(file);}

    void SkipSpaces()
    {
        while(cur_ind<cur_line_size)
        {
            char ch=line_buf[cur_ind];
            if(ch!=' ' && ch!='\t' && ch!='\r' && ch!='\n') break;
            cur_ind++;
        }
    }

    bool SkipUpto(const char* str)
    {
        while(true)
        {
            SkipSpaces();
            while(cur_ind>=cur_line_size) {if(!GetNewLine()) return false; SkipSpaces();}

            if(StartsWith(&line_buf[cur_ind], str))
            {
                cur_ind+=strlen(str);
                return true;
            }
            cur_ind++;
        }
        return false;
    }

    bool GetNewLine()
    {
        cur_ind=0; line_buf[0]=0;
        if(!fgets(line_buf, MAX_LINE_LENGTH, file)) return false;
        cur_line_size=strlen(line_buf);
        if(cur_line_size==0) return false; // End of file
        cur_line_num++;
        return true;
    }

    char* GetNextTokenStr()
    {
        SkipSpaces();
        while(cur_ind>=cur_line_size) {if(!GetNewLine()) return 0; SkipSpaces();}
        return &line_buf[cur_ind];
    }

    void Advance(int num)
    {
        cur_ind+=num;
    }
};

struct OutFile
{
    FILE* file;
    OutFile(const char* str) {file=0; if(str) file=fopen(str, "w");}
    ~OutFile(){if(file) fclose(file);}

    void Out(const char* s)
    {
        fprintf(file, "%s\n", s); fflush(file);
    }
};

////////////////////////////////////////////////////////////////////////////////////
// Compiler Parameters /////////////////////////////////////////////////////////////

struct CompilerInfo
{
    InFile in_file;
    OutFile out_file;
    OutFile debug_file;

    CompilerInfo(const char* in_str, const char* out_str, const char* debug_str)
                : in_file(in_str), out_file(out_str), debug_file(debug_str)
    {
    }
};

////////////////////////////////////////////////////////////////////////////////////
// Scanner /////////////////////////////////////////////////////////////////////////

#define MAX_TOKEN_LEN 40

enum TokenType{
                IF, THEN, ELSE, END, REPEAT, UNTIL, READ, WRITE,
                ASSIGN, EQUAL, LESS_THAN,
                PLUS, MINUS, TIMES, DIVIDE, POWER,
                SEMI_COLON,
                LEFT_PAREN, RIGHT_PAREN,
                LEFT_BRACE, RIGHT_BRACE,
                ID, NUM, REAL_NUM,
                INT, REAL, BOOL,
                ENDFILE, ERROR
              };

// Used for debugging only /////////////////////////////////////////////////////////
const char* TokenTypeStr[]=
            {
                "If", "Then", "Else", "End", "Repeat", "Until", "Read", "Write",
                "Assign", "Equal", "LessThan",
                "Plus", "Minus", "Times", "Divide", "Power",
                "SemiColon",
                "LeftParen", "RightParen",
                "LeftBrace", "RightBrace",
                "ID", "Num", "RealNum",
                "Int", "Real", "Bool",
                "EndFile", "Error"
            };

struct Token
{
    TokenType type;
    char str[MAX_TOKEN_LEN+1];

    Token(){str[0]=0; type=ERROR;}
    Token(TokenType _type, const char* _str) {type=_type; Copy(str, _str);}
};

const Token reserved_words[]=
{
    Token(IF, "if"),
    Token(THEN, "then"),
    Token(ELSE, "else"),
    Token(END, "end"),
    Token(REPEAT, "repeat"),
    Token(UNTIL, "until"),
    Token(READ, "read"),
    Token(WRITE, "write"),
    Token(INT, "int"),
    Token(REAL, "real"),
    Token(BOOL, "bool")
};
const int num_reserved_words=sizeof(reserved_words)/sizeof(reserved_words[0]);

// if there is tokens like < <=, sort them such that sub-tokens come last: <= <
// the closing comment should come immediately after opening comment
const Token symbolic_tokens[]=
{
    Token(ASSIGN, ":="),
    Token(EQUAL, "="),
    Token(LESS_THAN, "<"),
    Token(PLUS, "+"),
    Token(MINUS, "-"),
    Token(TIMES, "*"),
    Token(DIVIDE, "/"),
    Token(POWER, "^"),
    Token(SEMI_COLON, ";"),
    Token(LEFT_PAREN, "("),
    Token(RIGHT_PAREN, ")"),
    Token(LEFT_BRACE, "{"),
    Token(RIGHT_BRACE, "}")
};
const int num_symbolic_tokens=sizeof(symbolic_tokens)/sizeof(symbolic_tokens[0]);

inline bool IsDigit(char ch){return (ch>='0' && ch<='9');}
inline bool IsLetter(char ch){return ((ch>='a' && ch<='z') || (ch>='A' && ch<='Z'));}
inline bool IsLetterOrUnderscore(char ch){return (IsLetter(ch) || ch=='_');}

void GetNextToken(CompilerInfo* pci, Token* ptoken)
{
    ptoken->type=ERROR;
    ptoken->str[0]=0;

    int i;
    char* s=pci->in_file.GetNextTokenStr();
    if(!s)
    {
        ptoken->type=ENDFILE;
        ptoken->str[0]=0;
        return;
    }

    for(i=0;i<num_symbolic_tokens;i++)
    {
        if(StartsWith(s, symbolic_tokens[i].str))
            break;
    }

    if(i<num_symbolic_tokens)
    {
        if(symbolic_tokens[i].type==LEFT_BRACE)
        {
            pci->in_file.Advance(strlen(symbolic_tokens[i].str));
            if(!pci->in_file.SkipUpto(symbolic_tokens[i+1].str)) return;
            return GetNextToken(pci, ptoken);
        }
        ptoken->type=symbolic_tokens[i].type;
        Copy(ptoken->str, symbolic_tokens[i].str);
    }
    else if(IsDigit(s[0]))
    {
        int j=1;
        bool is_real=false;
        while(IsDigit(s[j])) j++;
        
        if(s[j]=='.')
        {
            is_real=true;
            j++;
            while(IsDigit(s[j])) j++;
        }

        ptoken->type=is_real ? REAL_NUM : NUM;
        Copy(ptoken->str, s, j);
    }
    else if(IsLetterOrUnderscore(s[0]))
    {
        int j=1;
        while(IsLetterOrUnderscore(s[j])) j++;

        ptoken->type=ID;
        Copy(ptoken->str, s, j);

        for(i=0;i<num_reserved_words;i++)
        {
            if(Equals(ptoken->str, reserved_words[i].str))
            {
                ptoken->type=reserved_words[i].type;
                break;
            }
        }
    }

    int len=strlen(ptoken->str);
    if(len>0) pci->in_file.Advance(len);
}

////////////////////////////////////////////////////////////////////////////////////
// Parser //////////////////////////////////////////////////////////////////////////

// program -> stmtseq
// stmtseq -> stmt { ; stmt }
// stmt -> declarestmt | ifstmt | repeatstmt | assignstmt | readstmt | writestmt 
// ifstmt -> if exp then stmtseq [ else stmtseq ] end
// repeatstmt -> repeat stmtseq until expr
// declarestmt -> (int|real|bool) identifier ;
// assignstmt -> identifier := expr
// readstmt -> read identifier
// writestmt -> write expr
// expr -> mathexpr [ (<|=) mathexpr ]
// mathexpr -> term { (+|-) term }    left associative
// term -> factor { (*|/) factor }    left associative
// factor -> newexpr { ^ newexpr }    right associative
// newexpr -> ( mathexpr ) | number | identifier

enum NodeKind{
                IF_NODE, REPEAT_NODE, ASSIGN_NODE, READ_NODE, WRITE_NODE,
                OPER_NODE, NUM_NODE, ID_NODE, DECLARE_NODE, REAL_NUM_NODE
             };

// Used for debugging only /////////////////////////////////////////////////////////
const char* NodeKindStr[]=
            {
                "If", "Repeat", "Assign", "Read", "Write",
                "Oper", "Num", "ID", "Declare", "RealNum"
            };

enum ExprDataType {VOID, INTEGER, REAL_TYPE, BOOLEAN};

// Used for debugging only /////////////////////////////////////////////////////////
const char* ExprDataTypeStr[]=
            {
                "Void", "Integer", "Real", "Boolean"
            };

#define MAX_CHILDREN 3

struct TreeNode
{
    TreeNode* child[MAX_CHILDREN];
    TreeNode* sibling; // used for sibling statements only

    NodeKind node_kind;

    union{TokenType oper; int num; double real_num; char* id;}; // defined for expression/int/identifier only
    ExprDataType expr_data_type; // defined for expression/int/identifier only
    TokenType decl_type; // for DECLARE_NODE: INT, REAL, or BOOL

    int line_num;

    TreeNode() {int i; for(i=0;i<MAX_CHILDREN;i++) child[i]=0; sibling=0; expr_data_type=VOID; decl_type=ERROR;}
};

struct ParseInfo
{
    Token next_token;
};

void Match(CompilerInfo* pci, ParseInfo* ppi, TokenType expected_token_type)
{
    pci->debug_file.Out("Start Match");

    if(ppi->next_token.type!=expected_token_type) throw 0;
    GetNextToken(pci, &ppi->next_token);

    fprintf(pci->debug_file.file, "[%d] %s (%s)\n", pci->in_file.cur_line_num, ppi->next_token.str, TokenTypeStr[ppi->next_token.type]); fflush(pci->debug_file.file);
}

TreeNode* MathExpr(CompilerInfo*, ParseInfo*);

// newexpr -> ( mathexpr ) | number | identifier
TreeNode* NewExpr(CompilerInfo* pci, ParseInfo* ppi)
{
    pci->debug_file.Out("Start NewExpr");

    // Compare the next token with the First() of possible statements
    if(ppi->next_token.type==NUM)
    {
        TreeNode* tree=new TreeNode;
        tree->node_kind=NUM_NODE;
        char* num_str=ppi->next_token.str;
        tree->num=0; while(*num_str) tree->num=tree->num*10+((*num_str++)-'0');
        tree->line_num=pci->in_file.cur_line_num;
        Match(pci, ppi, ppi->next_token.type);

        pci->debug_file.Out("End NewExpr");
        return tree;
    }

    if(ppi->next_token.type==REAL_NUM)
    {
        TreeNode* tree=new TreeNode;
        tree->node_kind=REAL_NUM_NODE;
        tree->real_num=atof(ppi->next_token.str);
        tree->line_num=pci->in_file.cur_line_num;
        Match(pci, ppi, ppi->next_token.type);

        pci->debug_file.Out("End NewExpr");
        return tree;
    }

    if(ppi->next_token.type==ID)
    {
        TreeNode* tree=new TreeNode;
        tree->node_kind=ID_NODE;
        AllocateAndCopy(&tree->id, ppi->next_token.str);
        tree->line_num=pci->in_file.cur_line_num;
        Match(pci, ppi, ppi->next_token.type);

        pci->debug_file.Out("End NewExpr");
        return tree;
    }

    if(ppi->next_token.type==LEFT_PAREN)
    {
        Match(pci, ppi, LEFT_PAREN);
        TreeNode* tree=MathExpr(pci, ppi);
        Match(pci, ppi, RIGHT_PAREN);

        pci->debug_file.Out("End NewExpr");
        return tree;
    }

    throw 0;
    return 0;
}

// factor -> newexpr { ^ newexpr }    right associative
TreeNode* Factor(CompilerInfo* pci, ParseInfo* ppi)
{
    pci->debug_file.Out("Start Factor");

    TreeNode* tree=NewExpr(pci, ppi);

    if(ppi->next_token.type==POWER)
    {
        TreeNode* new_tree=new TreeNode;
        new_tree->node_kind=OPER_NODE;
        new_tree->oper=ppi->next_token.type;
        new_tree->line_num=pci->in_file.cur_line_num;

        new_tree->child[0]=tree;
        Match(pci, ppi, ppi->next_token.type);
        new_tree->child[1]=Factor(pci, ppi);

        pci->debug_file.Out("End Factor");
        return new_tree;
    }
    pci->debug_file.Out("End Factor");
    return tree;
}

// term -> factor { (*|/) factor }    left associative
TreeNode* Term(CompilerInfo* pci, ParseInfo* ppi)
{
    pci->debug_file.Out("Start Term");

    TreeNode* tree=Factor(pci, ppi);

    while(ppi->next_token.type==TIMES || ppi->next_token.type==DIVIDE)
    {
        TreeNode* new_tree=new TreeNode;
        new_tree->node_kind=OPER_NODE;
        new_tree->oper=ppi->next_token.type;
        new_tree->line_num=pci->in_file.cur_line_num;

        new_tree->child[0]=tree;
        Match(pci, ppi, ppi->next_token.type);
        new_tree->child[1]=Factor(pci, ppi);

        tree=new_tree;
    }
    pci->debug_file.Out("End Term");
    return tree;
}

// mathexpr -> term { (+|-) term }    left associative
TreeNode* MathExpr(CompilerInfo* pci, ParseInfo* ppi)
{
    pci->debug_file.Out("Start MathExpr");

    TreeNode* tree=Term(pci, ppi);

    while(ppi->next_token.type==PLUS || ppi->next_token.type==MINUS)
    {
        TreeNode* new_tree=new TreeNode;
        new_tree->node_kind=OPER_NODE;
        new_tree->oper=ppi->next_token.type;
        new_tree->line_num=pci->in_file.cur_line_num;

        new_tree->child[0]=tree;
        Match(pci, ppi, ppi->next_token.type);
        new_tree->child[1]=Term(pci, ppi);

        tree=new_tree;
    }
    pci->debug_file.Out("End MathExpr");
    return tree;
}

// expr -> mathexpr [ (<|=) mathexpr ]
TreeNode* Expr(CompilerInfo* pci, ParseInfo* ppi)
{
    pci->debug_file.Out("Start Expr");

    TreeNode* tree=MathExpr(pci, ppi);

    if(ppi->next_token.type==EQUAL || ppi->next_token.type==LESS_THAN)
    {
        TreeNode* new_tree=new TreeNode;
        new_tree->node_kind=OPER_NODE;
        new_tree->oper=ppi->next_token.type;
        new_tree->line_num=pci->in_file.cur_line_num;

        new_tree->child[0]=tree;
        Match(pci, ppi, ppi->next_token.type);
        new_tree->child[1]=MathExpr(pci, ppi);

        pci->debug_file.Out("End Expr");
        return new_tree;
    }
    pci->debug_file.Out("End Expr");
    return tree;
}

// writestmt -> write expr
TreeNode* WriteStmt(CompilerInfo* pci, ParseInfo* ppi)
{
    pci->debug_file.Out("Start WriteStmt");

    TreeNode* tree=new TreeNode;
    tree->node_kind=WRITE_NODE;
    tree->line_num=pci->in_file.cur_line_num;

    Match(pci, ppi, WRITE);
    tree->child[0]=Expr(pci, ppi);

    pci->debug_file.Out("End WriteStmt");
    return tree;
}

// readstmt -> read identifier
TreeNode* ReadStmt(CompilerInfo* pci, ParseInfo* ppi)
{
    pci->debug_file.Out("Start ReadStmt");

    TreeNode* tree=new TreeNode;
    tree->node_kind=READ_NODE;
    tree->line_num=pci->in_file.cur_line_num;

    Match(pci, ppi, READ);
    if(ppi->next_token.type==ID) AllocateAndCopy(&tree->id, ppi->next_token.str);
    Match(pci, ppi, ID);

    pci->debug_file.Out("End ReadStmt");
    return tree;
}

// assignstmt -> identifier := expr
TreeNode* AssignStmt(CompilerInfo* pci, ParseInfo* ppi)
{
    pci->debug_file.Out("Start AssignStmt");

    TreeNode* tree=new TreeNode;
    tree->node_kind=ASSIGN_NODE;
    tree->line_num=pci->in_file.cur_line_num;

    if(ppi->next_token.type==ID) AllocateAndCopy(&tree->id, ppi->next_token.str);
    Match(pci, ppi, ID);
    Match(pci, ppi, ASSIGN); tree->child[0]=Expr(pci, ppi);

    pci->debug_file.Out("End AssignStmt");
    return tree;
}

TreeNode* StmtSeq(CompilerInfo*, ParseInfo*);

// declarestmt -> (int|real|bool) identifier ;
TreeNode* DeclareStmt(CompilerInfo* pci, ParseInfo* ppi)
{
    pci->debug_file.Out("Start DeclareStmt");

    TreeNode* tree=new TreeNode;
    tree->node_kind=DECLARE_NODE;
    tree->line_num=pci->in_file.cur_line_num;
    tree->decl_type=ppi->next_token.type;

    Match(pci, ppi, ppi->next_token.type); // INT, REAL, or BOOL
    if(ppi->next_token.type==ID) AllocateAndCopy(&tree->id, ppi->next_token.str);
    Match(pci, ppi, ID);

    pci->debug_file.Out("End DeclareStmt");
    return tree;
}

// repeatstmt -> repeat stmtseq until expr
TreeNode* RepeatStmt(CompilerInfo* pci, ParseInfo* ppi)
{
    pci->debug_file.Out("Start RepeatStmt");

    TreeNode* tree=new TreeNode;
    tree->node_kind=REPEAT_NODE;
    tree->line_num=pci->in_file.cur_line_num;

    Match(pci, ppi, REPEAT); tree->child[0]=StmtSeq(pci, ppi);
    Match(pci, ppi, UNTIL); tree->child[1]=Expr(pci, ppi);

    pci->debug_file.Out("End RepeatStmt");
    return tree;
}

// ifstmt -> if exp then stmtseq [ else stmtseq ] end
TreeNode* IfStmt(CompilerInfo* pci, ParseInfo* ppi)
{
    pci->debug_file.Out("Start IfStmt");

    TreeNode* tree=new TreeNode;
    tree->node_kind=IF_NODE;
    tree->line_num=pci->in_file.cur_line_num;

    Match(pci, ppi, IF); tree->child[0]=Expr(pci, ppi);
    Match(pci, ppi, THEN); tree->child[1]=StmtSeq(pci, ppi);
    if(ppi->next_token.type==ELSE) {Match(pci, ppi, ELSE); tree->child[2]=StmtSeq(pci, ppi);}
    Match(pci, ppi, END);

    pci->debug_file.Out("End IfStmt");
    return tree;
}

// stmt -> declarestmt | ifstmt | repeatstmt | assignstmt | readstmt | writestmt
TreeNode* Stmt(CompilerInfo* pci, ParseInfo* ppi)
{
    pci->debug_file.Out("Start Stmt");

    // Compare the next token with the First() of possible statements
    TreeNode* tree=0;
    if(ppi->next_token.type==INT || ppi->next_token.type==REAL || ppi->next_token.type==BOOL) tree=DeclareStmt(pci, ppi);
    else if(ppi->next_token.type==IF) tree=IfStmt(pci, ppi);
    else if(ppi->next_token.type==REPEAT) tree=RepeatStmt(pci, ppi);
    else if(ppi->next_token.type==ID) tree=AssignStmt(pci, ppi);
    else if(ppi->next_token.type==READ) tree=ReadStmt(pci, ppi);
    else if(ppi->next_token.type==WRITE) tree=WriteStmt(pci, ppi);
    else throw 0;

    pci->debug_file.Out("End Stmt");
    return tree;
}

// stmtseq -> stmt { ; stmt }
TreeNode* StmtSeq(CompilerInfo* pci, ParseInfo* ppi)
{
    pci->debug_file.Out("Start StmtSeq");

    TreeNode* first_tree=Stmt(pci, ppi);
    TreeNode* last_tree=first_tree;

    // If we did not reach one of the Follow() of StmtSeq(), we are not done yet
    while(ppi->next_token.type!=ENDFILE && ppi->next_token.type!=END &&
          ppi->next_token.type!=ELSE && ppi->next_token.type!=UNTIL)
    {
        Match(pci, ppi, SEMI_COLON);
        TreeNode* next_tree=Stmt(pci, ppi);
        last_tree->sibling=next_tree;
        last_tree=next_tree;
    }

    pci->debug_file.Out("End StmtSeq");
    return first_tree;
}

// program -> stmtseq
TreeNode* Parse(CompilerInfo* pci)
{
    ParseInfo parse_info;
    GetNextToken(pci, &parse_info.next_token);

    TreeNode* syntax_tree=StmtSeq(pci, &parse_info);

    if(parse_info.next_token.type!=ENDFILE)
        pci->debug_file.Out("Error code ends before file ends");

    return syntax_tree;
}

void PrintTree(TreeNode* node, int sh=0)
{
    int i, NSH=3;
    for(i=0;i<sh;i++) printf(" ");

    printf("[%s]", NodeKindStr[node->node_kind]);

    if(node->node_kind==OPER_NODE) printf("[%s]", TokenTypeStr[node->oper]);
    else if(node->node_kind==NUM_NODE) printf("[%d]", node->num);
    else if(node->node_kind==REAL_NUM_NODE) printf("[%f]", node->real_num);
    else if(node->node_kind==ID_NODE || node->node_kind==READ_NODE || node->node_kind==ASSIGN_NODE) printf("[%s]", node->id);
    else if(node->node_kind==DECLARE_NODE) printf("[%s][%s]", TokenTypeStr[node->decl_type], node->id);

    if(node->expr_data_type!=VOID) printf("[%s]", ExprDataTypeStr[node->expr_data_type]);

    printf("\n");

    for(i=0;i<MAX_CHILDREN;i++) if(node->child[i]) PrintTree(node->child[i], sh+NSH);
    if(node->sibling) PrintTree(node->sibling, sh);
}

void DestroyTree(TreeNode* node)
{
    int i;

    if(node->node_kind==ID_NODE || node->node_kind==READ_NODE || node->node_kind==ASSIGN_NODE || node->node_kind==DECLARE_NODE)
        if(node->id) delete[] node->id;

    for(i=0;i<MAX_CHILDREN;i++) if(node->child[i]) DestroyTree(node->child[i]);
    if(node->sibling) DestroyTree(node->sibling);

    delete node;
}

////////////////////////////////////////////////////////////////////////////////////
// Analyzer ////////////////////////////////////////////////////////////////////////

const int SYMBOL_HASH_SIZE=10007;

struct LineLocation
{
    int line_num;
    LineLocation* next;
};
struct VariableInfo
{
    char* name;
    int memloc;
    ExprDataType var_type; // INTEGER, REAL_TYPE, or BOOLEAN
    bool is_declared;
    LineLocation* head_line; // the head of linked list of source line locations
    LineLocation* tail_line; // the tail of linked list of source line locations
    VariableInfo* next_var; // the next variable in the linked list in the same hash bucket of the symbol table
};

struct SymbolTable
{
    int num_vars;
    VariableInfo* var_info[SYMBOL_HASH_SIZE];

    SymbolTable() {num_vars=0; int i; for(i=0;i<SYMBOL_HASH_SIZE;i++) var_info[i]=0;}

    int Hash(const char* name)
    {
        int i, len=strlen(name);
        int hash_val=11;
        for(i=0;i<len;i++) hash_val=(hash_val*17+(int)name[i])%SYMBOL_HASH_SIZE;
        return hash_val;
    }

    VariableInfo* Find(const char* name)
    {
        int h=Hash(name);
        VariableInfo* cur=var_info[h];
        while(cur)
        {
            if(Equals(name, cur->name)) return cur;
            cur=cur->next_var;
        }
        return 0;
    }

    void Insert(const char* name, int line_num, ExprDataType type=VOID)
    {
        LineLocation* lineloc=new LineLocation;
        lineloc->line_num=line_num;
        lineloc->next=0;

        int h=Hash(name);
        VariableInfo* prev=0;
        VariableInfo* cur=var_info[h];
    
        while(cur)
        {
            if(Equals(name, cur->name))
            {
                // just add this line location to the list of line locations of the existing var
                cur->tail_line->next=lineloc;
                cur->tail_line=lineloc;
                return;
            }
            prev=cur;
            cur=cur->next_var;
        }

        VariableInfo* vi=new VariableInfo;
        vi->head_line=vi->tail_line=lineloc;
        vi->next_var=0;
        vi->memloc=num_vars++;
        vi->var_type=type;
        vi->is_declared=(type!=VOID);
        AllocateAndCopy(&vi->name, name);

        if(!prev) var_info[h]=vi;
        else prev->next_var=vi;
    }

    void Declare(const char* name, ExprDataType type, int line_num)
    {
        VariableInfo* vi=Find(name);
        if(vi)
        {
            if(vi->is_declared)
            {
                printf("ERROR: Variable '%s' already declared at line %d\n", name, line_num);
                throw 0;
            }
            vi->var_type=type;
            vi->is_declared=true;
        }
        else
        {
            Insert(name, line_num, type);
        }
    }

    void Print()
    {
        int i;
        for(i=0;i<SYMBOL_HASH_SIZE;i++)
        {
            VariableInfo* curv=var_info[i];
            while(curv)
            {
                printf("[Var=%s][Type=%s][Mem=%d]", curv->name, ExprDataTypeStr[curv->var_type], curv->memloc);
                LineLocation* curl=curv->head_line;
                while(curl)
                {
                    printf("[Line=%d]", curl->line_num);
                    curl=curl->next;
                }
                printf("\n");
                curv=curv->next_var;
            }
        }
    }

    void Destroy()
    {
        int i;
        for(i=0;i<SYMBOL_HASH_SIZE;i++)
        {
            VariableInfo* curv=var_info[i];
            while(curv)
            {
                LineLocation* curl=curv->head_line;
                while(curl)
                {
                    LineLocation* pl=curl;
                    curl=curl->next;
                    delete pl;
                }
                VariableInfo* p=curv;
                curv=curv->next_var;
                delete p;
            }
            var_info[i]=0;
        }
    }
};

void Analyze(TreeNode* node, SymbolTable* symbol_table)
{
    int i;

    // Handle declarations
    if(node->node_kind==DECLARE_NODE)
    {
        ExprDataType type=VOID;
        if(node->decl_type==INT) type=INTEGER;
        else if(node->decl_type==REAL) type=REAL_TYPE;
        else if(node->decl_type==BOOL) type=BOOLEAN;
        symbol_table->Declare(node->id, type, node->line_num);
    }
    
    // Handle variable references
    if(node->node_kind==ID_NODE || node->node_kind==READ_NODE || node->node_kind==ASSIGN_NODE)
        symbol_table->Insert(node->id, node->line_num);

    // Recursively analyze children
    for(i=0;i<MAX_CHILDREN;i++) if(node->child[i]) Analyze(node->child[i], symbol_table);

    // Determine expression types
    if(node->node_kind==NUM_NODE) 
        node->expr_data_type=INTEGER;
    else if(node->node_kind==REAL_NUM_NODE) 
        node->expr_data_type=REAL_TYPE;
    else if(node->node_kind==ID_NODE)
    {
        VariableInfo* vi=symbol_table->Find(node->id);
        if(!vi)
        {
            printf("ERROR: Variable '%s' used but not declared at line %d\n", node->id, node->line_num);
            throw 0;
        }
        if(!vi->is_declared)
        {
            printf("ERROR: Variable '%s' used before declaration at line %d\n", node->id, node->line_num);
            throw 0;
        }
        node->expr_data_type=vi->var_type;
    }
    else if(node->node_kind==OPER_NODE)
    {
        if(node->oper==EQUAL || node->oper==LESS_THAN)
        {
            // Comparison operators
            ExprDataType left=node->child[0]->expr_data_type;
            ExprDataType right=node->child[1]->expr_data_type;
            
            if(left==BOOLEAN || right==BOOLEAN)
            {
                printf("ERROR: cannot compare boolean values at line %d\n", node->line_num);
                throw 0;
            }
            node->expr_data_type=BOOLEAN;
        }
        else
        {
            // Arithmetic operators
            ExprDataType left=node->child[0]->expr_data_type;
            ExprDataType right=node->child[1]->expr_data_type;
            
            if(left==BOOLEAN || right==BOOLEAN)
            {
                printf("ERROR: Cannot perform arithmetic on boolean values at line %d\n", node->line_num);
                throw 0;
            }
            
            // If either operand is REAL, result is REAL
            if(left==REAL_TYPE || right==REAL_TYPE)
                node->expr_data_type=REAL_TYPE;
            else
                node->expr_data_type=INTEGER;
        }
    }

    // Type checking for statements
    if(node->node_kind==IF_NODE)
    {
        if(node->child[0]->expr_data_type!=BOOLEAN)
        {
            printf("ERROR: If condition must be BOOLEAN at line %d\n", node->line_num);
            throw 0;
        }
    }
    
    if(node->node_kind==REPEAT_NODE)
    {
        if(node->child[1]->expr_data_type!=BOOLEAN)
        {
            printf("ERROR: Repeat condition must be BOOLEAN at line %d\n", node->line_num);
            throw 0;
        }
    }
    
    if(node->node_kind==ASSIGN_NODE)
    {
        VariableInfo* vi=symbol_table->Find(node->id);
        if(!vi || !vi->is_declared)
        {
            printf("ERROR: Cannot assign to undeclared variable '%s' at line %d\n", node->id, node->line_num);
            throw 0;
        }
        
        ExprDataType var_type=vi->var_type;
        ExprDataType expr_type=node->child[0]->expr_data_type;

        // Check type compatibility
        if(var_type!=expr_type)
        {
            // Allow int to real conversion in assignment
            if(!(var_type==REAL_TYPE && expr_type==INTEGER || var_type==INTEGER && expr_type==REAL_TYPE)) //here
            {
                printf("ERROR: Type mismatch in assignment to '%s' at line %d. Expected %s but got %s\n", 
                       node->id, node->line_num, ExprDataTypeStr[var_type], ExprDataTypeStr[expr_type]);
                throw 0;
            }
        }
    }
    
    if(node->node_kind==READ_NODE)
    {
        VariableInfo* vi=symbol_table->Find(node->id);
        if(!vi || !vi->is_declared)
        {
            printf("ERROR: Cannot read into undeclared variable '%s' at line %d\n", node->id, node->line_num);
            throw 0;
        }
    }
    
    if(node->node_kind==WRITE_NODE)
    {
        ExprDataType type=node->child[0]->expr_data_type;
        if(type!=INTEGER && type!=REAL_TYPE)
        {
            printf("ERROR: Write statement requires INTEGER or REAL expression at line %d\n", node->line_num);
            throw 0;
        }
    }

    if(node->sibling) Analyze(node->sibling, symbol_table);
}

////////////////////////////////////////////////////////////////////////////////////
// Code Generator //////////////////////////////////////////////////////////////////

int Power(int a, int b)
{
    if(a==0) return 0;
    if(b==0) return 1;
    if(b>=1) return a*Power(a, b-1);
    return 0;
}

struct Value
{
    ExprDataType type;
    union {int int_val; double real_val; bool bool_val;};
    
    Value() : type(VOID), int_val(0) {}
    Value(int v) : type(INTEGER), int_val(v) {}
    Value(double v) : type(REAL_TYPE), real_val(v) {}
    Value(bool v) : type(BOOLEAN), bool_val(v) {}
};

Value Evaluate(TreeNode* node, SymbolTable* symbol_table, int* int_vars, double* real_vars, bool* bool_vars)
{
    if(node->node_kind==NUM_NODE) return Value(node->num);
    if(node->node_kind==REAL_NUM_NODE) return Value(node->real_num);
    
    if(node->node_kind==ID_NODE)
    {
        VariableInfo* vi=symbol_table->Find(node->id);
        if(vi->var_type==INTEGER) return Value(int_vars[vi->memloc]);
        if(vi->var_type==REAL_TYPE) return Value(real_vars[vi->memloc]);
        if(vi->var_type==BOOLEAN) return Value(bool_vars[vi->memloc]);
    }

    Value a=Evaluate(node->child[0], symbol_table, int_vars, real_vars, bool_vars);
    Value b=Evaluate(node->child[1], symbol_table, int_vars, real_vars, bool_vars);

    if(node->oper==EQUAL || node->oper==LESS_THAN)
    {
        double val_a=(a.type==INTEGER ? a.int_val : a.real_val);
        double val_b=(b.type==INTEGER ? b.int_val : b.real_val);
        
        if(node->oper==EQUAL) return Value(val_a==val_b);
        if(node->oper==LESS_THAN) return Value(val_a<val_b);
    }
    
    // Arithmetic operations
    bool result_is_real=(a.type==REAL_TYPE || b.type==REAL_TYPE);
    double val_a=(a.type==INTEGER ? a.int_val : a.real_val);
    double val_b=(b.type==INTEGER ? b.int_val : b.real_val);
    
    double result=0;
    if(node->oper==PLUS) result=val_a+val_b;
    else if(node->oper==MINUS) result=val_a-val_b;
    else if(node->oper==TIMES) result=val_a*val_b;
    else if(node->oper==DIVIDE) result=val_a/val_b;
    else if(node->oper==POWER)
    {
        result=1;
        int exp=(int)val_b;
        for(int i=0;i<exp;i++) result*=val_a;
    }
    
    if(result_is_real) return Value(result);
    else return Value((int)result);
}

void RunProgram(TreeNode* node, SymbolTable* symbol_table, int* int_vars, double* real_vars, bool* bool_vars)
{
    if(node->node_kind==IF_NODE)
    {
        Value cond=Evaluate(node->child[0], symbol_table, int_vars, real_vars, bool_vars);
        if(cond.bool_val) RunProgram(node->child[1], symbol_table, int_vars, real_vars, bool_vars);
        else if(node->child[2]) RunProgram(node->child[2], symbol_table, int_vars, real_vars, bool_vars);
    }
    else if(node->node_kind==ASSIGN_NODE)
    {
        Value v=Evaluate(node->child[0], symbol_table, int_vars, real_vars, bool_vars);
        VariableInfo* vi=symbol_table->Find(node->id);
        
        if(vi->var_type==INTEGER)
            int_vars[vi->memloc]=(v.type==INTEGER ? v.int_val : (int)v.real_val);
        else if(vi->var_type==REAL_TYPE)
            real_vars[vi->memloc]=(v.type==REAL_TYPE ? v.real_val : (double)v.int_val);
        else if(vi->var_type==BOOLEAN)
            bool_vars[vi->memloc]=v.bool_val;
    }
    else if(node->node_kind==READ_NODE)
    {
        VariableInfo* vi=symbol_table->Find(node->id);
        printf("Enter %s: ", node->id);
        
        if(vi->var_type==INTEGER)
            scanf("%d", &int_vars[vi->memloc]);
        else if(vi->var_type==REAL_TYPE)
            scanf("%lf", &real_vars[vi->memloc]);
        else if(vi->var_type==BOOLEAN)
        {
            int b;
            scanf("%d", &b);
            bool_vars[vi->memloc]=(b!=0);
        }
    }
    else if(node->node_kind==WRITE_NODE)
    {
        Value v=Evaluate(node->child[0], symbol_table, int_vars, real_vars, bool_vars);
        if(v.type==INTEGER)
            printf("Val: %d\n", v.int_val);
        else if(v.type==REAL_TYPE)
            printf("Val: %f\n", v.real_val);
    }
    else if(node->node_kind==REPEAT_NODE)
    {
        do
        {
           RunProgram(node->child[0], symbol_table, int_vars, real_vars, bool_vars);
        }
        while(!Evaluate(node->child[1], symbol_table, int_vars, real_vars, bool_vars).bool_val);
    }
    
    if(node->sibling) RunProgram(node->sibling, symbol_table, int_vars, real_vars, bool_vars);
}

void RunProgram(TreeNode* syntax_tree, SymbolTable* symbol_table)
{
    int i;
    int* int_vars=new int[symbol_table->num_vars];
    double* real_vars=new double[symbol_table->num_vars];
    bool* bool_vars=new bool[symbol_table->num_vars];
    
    for(i=0;i<symbol_table->num_vars;i++)
    {
        int_vars[i]=0;
        real_vars[i]=0.0;
        bool_vars[i]=false;
    }
    
    RunProgram(syntax_tree, symbol_table, int_vars, real_vars, bool_vars);
    
    delete[] int_vars;
    delete[] real_vars;
    delete[] bool_vars;
}

////////////////////////////////////////////////////////////////////////////////////
// Scanner and Compiler ////////////////////////////////////////////////////////////

void StartCompiler(CompilerInfo* pci)
{
    TreeNode* syntax_tree=Parse(pci);

    SymbolTable symbol_table;
    Analyze(syntax_tree, &symbol_table);

    printf("Symbol Table:\n");
    symbol_table.Print();
    printf("---------------------------------\n"); fflush(NULL);

    printf("Syntax Tree:\n");
    PrintTree(syntax_tree);
    printf("---------------------------------\n"); fflush(NULL);

    printf("Run Program:\n");
    RunProgram(syntax_tree, &symbol_table);
    printf("---------------------------------\n"); fflush(NULL);

    symbol_table.Destroy();
    DestroyTree(syntax_tree);
}

////////////////////////////////////////////////////////////////////////////////////
// Scanner only ////////////////////////////////////////////////////////////////////

void StartScanner(CompilerInfo* pci)
{
    Token token;

    while(true)
    {
        GetNextToken(pci, &token);
        printf("[%d] %s (%s)\n", pci->in_file.cur_line_num, token.str, TokenTypeStr[token.type]); fflush(NULL);
        if(token.type==ENDFILE || token.type==ERROR) break;
    }
}

////////////////////////////////////////////////////////////////////////////////////

int main()
{
    printf("Start main()\n"); fflush(NULL);

    CompilerInfo compiler_info("input.txt", "output.txt", "debug.txt");

    StartCompiler(&compiler_info);

    printf("End main()\n"); fflush(NULL);
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////


/*
    int x;
int y;
int z;
real a;
real b;
real c;
bool flag;
bool check;

{ Case 1: Integer Assignment }
x := 10;
write x;

{ Case 2: Integer Assignment }
y := 2;
write y;

{ Case 3: Real Assignment }
a := 5.5;
write a;

{ Case 4: Real Assignment }
b := 2.0;
write b;

{ Case 5: Boolean Assignment via Comparison (<) }
{ Validates that comparison returns a boolean type }
flag := x < y;
{ write flag; }

{ Case 6: Boolean Assignment via Equality (=) }
check := x = 10;

{ Case 7: Integer Addition }
z := x + y;
write z;

{ Case 8: Integer Subtraction }
z := x - y;
write z;

{ Case 9: Integer Multiplication }
z := x * y;
write z;

{ Case 10: Integer Division }
z := x / y;
write z;

{ Case 11: Real Arithmetic }
c := a + b;
write c;

{ Case 12: Mixed Mode Addition (Int + Real) }
{ 'x' (int) should convert to real to match 'a' }
c := x + a;
write c;

{ Case 13: Mixed Mode Multiplication (Real * Int) }
{ 'y' (int) should convert to real }
c := a * y;
write c;

{ Case 14: Operator Precedence (Multiplication over Addition) }
{ 10 + 2 * 3 = 16 (not 36) }
z := x + y * 3;
write z;

{ Case 15: Operator Precedence (Parentheses) }
{ (10 + 2) * 3 = 36 }
z := (x + y) * 3;
write z;

{ Case 16: Right Associativity of Power (^) }
{ 2 ^ 3 ^ 2 should correspond to 2 ^ 9 = 512 }
z := 2 ^ 3 ^ 2;
write z;

{ Case 17: Complex Mixed Expression }
{ (Real + Int) / Real }
c := (a + x) / b;
write c;

{ Case 18: If-Statement with Boolean Variable }
if check then
  write x
end;

{ Case 19: Write Statement with Expression }
write x + y;


{ Case 20: Repeat-Until with Boolean Expression }
repeat
  write x;
  x := x - 1
until x < 0
*/