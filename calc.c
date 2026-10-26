// Diana Hatsenko 260ADB164
// https://github.com/dianahatsenko/graded_lab
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: 
// https://www.geeksforgeeks.org/compiler-design/recursive-descent-parser/
// https://introcs-python.cs.luc.edu/recursion/recursiondparser.html
// https://codingeasily.in/tutorial/c/c-dynamic-memory/
// https://cppreference.com/c/io
// https://www.codewithc.in/tutorials/file-io
// https://cppreference.com/c/header/string
// https://www.geeksforgeeks.org/cpp/command-line-arguments-in-c-cpp/
// https://www.baeldung.com/linux/list-files-using-c


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>

#define FIRST_NAME "diana"
#define LAST_NAME  "hatsenko"
#define STUDENT_ID "260ADB164"

enum TokenType {
    T_END, T_NUMBER, T_PLUS, T_MINUS, T_MUL, T_DIV,
    T_POW, T_LEFT, T_RIGHT, T_BAD
};

struct Token {
    enum TokenType type;
    int pos;
    int isFloat;
    long long integer;
    double real;
};


static char *source;
static int at;
static int lineEnd;
static int errorPos;
static struct Token token;

static char *answer;
static int answerLen;
static int answerCap;

static int isDigit(char c)
{
    return c >= '0' && c <= '9';
}

static int isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\r' ||
           c == '\v' || c == '\f';
}

// Token positions use indexes in the original file
static void nextToken(void)
{
    int start;
    int isFloat;
    int numberOverflow = 0;
    long long number;
    double real;

    while (at < lineEnd && isSpace(source[at]))
        at++;

    token.pos = at + 1;
    token.isFloat = 0;
    token.integer = 0;
    token.real = 0.0;

    if (at >= lineEnd) {
        token.type = T_END;
        return;
    }

    start = at;
    if (isDigit(source[at])) {
        number = 0;
        real = 0.0;
        while (at < lineEnd && isDigit(source[at])) {
            int digit = source[at] - '0';
            // Check if number goes in overflow
            if (!numberOverflow) {
                if (number > (9223372036854775807LL - digit) / 10)
                    numberOverflow = 1;
                else
                    number = number * 10 + digit;
            }
            real = real * 10.0 + digit;
            at++;
        }

        // Handle float number
        isFloat = 0;
        if (at < lineEnd && source[at] == '.') {
            int dot = at;
            double part = 0.1;
            at++;
            if (at >= lineEnd || !isDigit(source[at])) {
                token.type = T_BAD;
                token.pos = dot + 1;
                return;
            }
            isFloat = 1;
            while (at < lineEnd && isDigit(source[at])) {
                real += (source[at] - '0') * part;
                part *= 0.1;
                at++;
            }
        }

        token.type = T_NUMBER;
        token.isFloat = isFloat;
        if (numberOverflow && !isFloat) {
            token.type = T_BAD;
            token.pos = start + 1;
            return;
        }
        token.integer = number;
        token.real = real;
        if (isFloat && !isfinite(real)) {
            token.type = T_BAD;
            token.pos = start + 1;
        }
        return;
    }

    switch (source[at]) {
        // Unknown characters are kept as T_BAD
        case '+': token.type = T_PLUS; at++; break;
        case '-': token.type = T_MINUS; at++; break;
        case '/': token.type = T_DIV; at++; break;
        case '(': token.type = T_LEFT; at++; break;
        case ')': token.type = T_RIGHT; at++; break;
        case '*':
            if (at + 1 < lineEnd && source[at + 1] == '*') {
                token.type = T_POW;
                at += 2;
            } else {
                token.type = T_MUL;
                at++;
            }
            break;
        default:
            token.type = T_BAD;
            at++;
            break;
    }
}

static struct Token makeInt(long long n)
{
    struct Token v;
    v.type = T_NUMBER;
    v.pos = 0;
    v.isFloat = 0;
    v.integer = n;
    v.real = (double)n;
    return v;
}

static struct Token makeFloat(double n)
{
    struct Token v;
    v.type = T_NUMBER;
    v.pos = 0;
    v.isFloat = 1;
    v.integer = 0;
    v.real = n;
    return v;
}

// The parser is split by precedence: expression handles +/-, term handles multiplication/division, and power handles **. Parentheses return to expression, while power
// parses its right side recursively so a ** b ** c means a ** (b ** c)
// Logic took from introcs-python.cs.luc.edu (resource 2)
static struct Token expression(void);

static struct Token primary(void)
{
    struct Token value;

    if (token.type == T_NUMBER) {
        if (token.isFloat)
            value = makeFloat(token.real);
        else
            value = makeInt(token.integer);
        nextToken();
        return value;
    }

    if (token.type == T_LEFT) {
        nextToken();
        value = expression();
        if (errorPos)
            return value;
        if (token.type == T_RIGHT) {
            nextToken();
            return value;
        }
        // The first token where parsing cannot continue is the missing ')' position 
        errorPos = token.pos;
        return makeInt(0);
    }

    // This also puts missing operands at ')' or at the line-ending position
    errorPos = token.pos;
    return makeInt(0);
}

// The recursive call on the right makes exponentiation right-associative
static struct Token power(void)
{
    struct Token left = primary();

    if (errorPos)
        return left;

    if (token.type == T_POW) {
        struct Token right;
        int opPos = token.pos;
        int expPos;

        nextToken();
        expPos = token.pos;
        right = power();
        if (errorPos)
            return left;

        if (!left.isFloat && !right.isFloat) {
            long long base = left.integer;
            long long exponent = right.integer;
            long long result = 1;

            if (exponent < 0) {
                // The specified position is the first token of the exponent
                errorPos = expPos;
                return makeInt(0);
            }
            while (exponent > 0) {
                if (exponent % 2)
                    result *= base;
                exponent /= 2;
                if (exponent > 0)
                    base *= base;
            }
            left = makeInt(result);
        } else {
            double result = pow(left.real, right.real);
            if (!isfinite(result)) {
                // A non-finite floating result is blamed on the operator
                errorPos = opPos;
                return makeInt(0);
            }
            left = makeFloat(result);
        }
    }
    return left;
}

static struct Token term(void)
{
    struct Token left = power();

    while (!errorPos && (token.type == T_MUL || token.type == T_DIV)) {
        struct Token op = token;
        struct Token right;
        int rightPos;

        nextToken();
        rightPos = token.pos;
        right = power();
        if (errorPos)
            return left;

        if (op.type == T_DIV &&
            (right.isFloat ? right.real == 0.0 : right.integer == 0)) {
            // Division by zero is attributed to the beginning of its right operand
            errorPos = rightPos;
            return makeInt(0);
        }

        // Keep exact integer arithmetic unless one of the operands is a float
        if (left.isFloat || right.isFloat) {
            double result;
            if (op.type == T_MUL)
                result = left.real * right.real;
            else
                result = left.real / right.real;
            if (!isfinite(result)) {
                errorPos = op.pos;
                return makeInt(0);
            }
            left = makeFloat(result);
        } else if (op.type == T_MUL) {
            left = makeInt(left.integer * right.integer);
        } else {
            left = makeInt(left.integer / right.integer);
        }
    }
    return left;
}

static struct Token expression(void)
{
    struct Token left = term();

    while (!errorPos && (token.type == T_PLUS || token.type == T_MINUS)) {
        struct Token op = token;
        struct Token right;

        nextToken();
        right = term();
        if (errorPos)
            return left;

        if (left.isFloat || right.isFloat) {
            double result;
            if (op.type == T_PLUS)
                result = left.real + right.real;
            else
                result = left.real - right.real;
            if (!isfinite(result)) {
                errorPos = op.pos;
                return makeInt(0);
            }
            left = makeFloat(result);
        } else if (op.type == T_PLUS) {
            left = makeInt(left.integer + right.integer);
        } else {
            left = makeInt(left.integer - right.integer);
        }
    }
    return left;
}

// Buffering keeps earlier answers out of the output file if a later line fails
// Doubling the buffer makes appending many short result lines linear overall
static int addAnswer(const char *line, int count)
{
    char *newAnswer;
    int newCap;

    if (answerLen + count + 1 > answerCap) {
        newCap = answerCap ? answerCap : 128;
        while (newCap < answerLen + count + 1)
            newCap *= 2;
        newAnswer = realloc(answer, newCap);
        if (!newAnswer)
            return 0;
        answer = newAnswer;
        answerCap = newCap;
    }
    memcpy(answer + answerLen, line, count);
    answerLen += count;
    answer[answerLen] = '\0';
    return 1;
}

static int addValue(struct Token value)
{
    char line[128];
    int count;

    if (value.isFloat)
        count = sprintf(line, "%.15g\n", value.real);
    else
        count = sprintf(line, "%lld\n", value.integer);
    return addAnswer(line, count);
}

// Read the file in blocks
// Functions from official C reference webpage (resource 4)
// with examples from codewithc.in (resource 5)
static char *readFile(const char *name, int *length)
{
    FILE *file = fopen(name, "rb");
    char *data;
    int used = 0;
    int cap = 4096;

    if (!file)
        return NULL;

    data = malloc(cap);
    if (!data) {
        fclose(file);
        return NULL;
    }

    for (;;) {
        int got;
        if (used + 2048 + 1 > cap) {
            char *bigger;
            cap *= 2;
            bigger = realloc(data, cap);
            if (!bigger) {
                free(data);
                fclose(file);
                return NULL;
            }
            data = bigger;
        }
        got = (int)fread(data + used, 1, cap - used - 1, file);
        used += got;
        if (got == 0) {
            if (ferror(file)) {
                free(data);
                fclose(file);
                return NULL;
            }
            break;
        }
    }

    data[used] = '\0';
    fclose(file);
    *length = used;
    return data;
}

static int calculateFile(const char *input, const char *outDir)
{
    char *data;
    char *base;
    char *name;
    char *path;
    FILE *out;
    int length;
    int start;
    int fileError = 0;
    int pathLen;

    data = readFile(input, &length);
    if (!data) {
        fprintf(stderr, "Cannot read input file: %s\n", input);
        return 0;
    }

    free(answer);
    answer = NULL;
    answerLen = 0;
    answerCap = 0;
    source = data;

    start = 0;
    while (start < length) {
        int end = start;
        int first = start;
        struct Token result;

        while (end < length && data[end] != '\n')
            end++;
        while (first < end && isSpace(data[first]))
            first++;

        if (first < end && data[first] != '#') {
            at = start;
            lineEnd = end;
            errorPos = 0;
            nextToken();
            result = expression();

            // Any leftover token is the first error
            if (!errorPos && token.type != T_END)
                errorPos = token.pos;

            if (errorPos) {
                fileError = errorPos;
                break;
            }
            if (!addValue(result)) {
                fprintf(stderr, "Not enough memory for results\n");
                free(data);
                return 0;
            }
        }

        start = end < length ? end + 1 : end;
    }

    if (fileError) {
        char line[64];
        int count = sprintf(line, "ERROR:%d\n", fileError);
        answerLen = 0;
        if (!addAnswer(line, count)) {
            fprintf(stderr, "Not enough memory for error result\n");
            free(data);
            return 0;
        }
    }

    base = strrchr(input, '/');
    base = base ? base + 1 : (char *)input;
    name = malloc((int)strlen(base) + 1);
    if (!name) {
        fprintf(stderr, "Not enough memory for output name\n");
        free(data);
        return 0;
    }
    strcpy(name, base);
    if (strlen(name) >= 4 && strcmp(name + strlen(name) - 4, ".txt") == 0)
        name[strlen(name) - 4] = '\0';

    pathLen = (int)strlen(outDir) + (int)strlen(name) +
              (int)strlen(FIRST_NAME) + (int)strlen(LAST_NAME) +
              (int)strlen(STUDENT_ID) + 10;
    path = malloc(pathLen);
    if (!path) {
        fprintf(stderr, "Not enough memory for output path\n");
        free(name);
        free(data);
        return 0;
    }
    sprintf(path, "%s/%s_%s_%s_%s.txt", outDir, name,
            FIRST_NAME, LAST_NAME, STUDENT_ID);

    out = fopen(path, "wb");
    if (!out) {
        fprintf(stderr, "Cannot write output file: %s\n", path);
        free(path);
        free(name);
        free(data);
        return 0;
    }
    if (answerLen && fwrite(answer, 1, answerLen, out) != (unsigned)answerLen) {
        fprintf(stderr, "Error while writing output file: %s\n", path);
        fclose(out);
        free(path);
        free(name);
        free(data);
        return 0;
    }
    if (fclose(out) != 0) {
        fprintf(stderr, "Error while closing output file: %s\n", path);
        free(path);
        free(name);
        free(data);
        return 0;
    }

    free(path);
    free(name);
    free(data);
    return 1;
}

static char *baseName(const char *path)
{
    const char *end = path + strlen(path);
    const char *begin;
    while (end > path && end[-1] == '/')
        end--;
    begin = end;
    while (begin > path && begin[-1] != '/')
        begin--;
    {
        int len = (int)(end - begin);
        char *name = malloc(len + 1);
        if (!name)
            return NULL;
        memcpy(name, begin, len);
        name[len] = '\0';
        return name;
    }
}

// String manipulation functions from official
// string.h library reference (resource 6)
static char *defaultDir(const char *input, int isDir)
{
    char *base = baseName(input);
    char *result;
    int len;

    if (!base)
        return NULL;
    len = (int)strlen(base);
    if (!isDir && len >= 4 && strcmp(base + len - 4, ".txt") == 0)
        base[len - 4] = '\0';

    len = (int)strlen(base) + (int)strlen(FIRST_NAME) +
          (int)strlen(STUDENT_ID) + 3;
    result = malloc(len);
    if (result) {
        strcpy(result, base);
        strcat(result, "_");
        strcat(result, FIRST_NAME);
        strcat(result, "_");
        strcat(result, STUDENT_ID);
    }
    free(base);
    return result;
}

// Directory manipulation on Linux with dirent.h and sys/stat.h
// (last resource)
static int makeOutputDir(const char *dir)
{
    struct stat info;
    if (stat(dir, &info) == 0) {
        if (S_ISDIR(info.st_mode))
            return 1;
        fprintf(stderr, "Output path is not a directory: %s\n", dir);
        return 0;
    }
    if (mkdir(dir, 0755) != 0) {
        fprintf(stderr, "Cannot create output directory: %s\n", dir);
        return 0;
    }
    return 1;
}

static int processDir(const char *dir, const char *outDir)
{
    DIR *folder = opendir(dir);
    struct dirent *item;
    int ok = 1;

    if (!folder) {
        fprintf(stderr, "Cannot open input directory: %s\n", dir);
        return 0;
    }

    while ((item = readdir(folder)) != NULL) {
        int len = (int)strlen(item->d_name);
        char *path;
        struct stat info;
        int pathLen;

        if (len < 4 || strcmp(item->d_name + len - 4, ".txt") != 0)
            continue;

        pathLen = (int)strlen(dir) + len + 2;
        path = malloc(pathLen);
        if (!path) {
            fprintf(stderr, "Not enough memory for input path\n");
            ok = 0;
            break;
        }
        if (dir[strlen(dir) - 1] == '/')
            sprintf(path, "%s%s", dir, item->d_name);
        else
            sprintf(path, "%s/%s", dir, item->d_name);

        if (stat(path, &info) == 0 && S_ISREG(info.st_mode)) {
            if (!calculateFile(path, outDir))
                ok = 0;
        }
        free(path);
        if (!ok)
            break;
    }

    closedir(folder);
    return ok;
}

int main(int argc, char **argv)
{
    char *dir = NULL;
    char *outDir = NULL;
    char *input = NULL;
    int i;
    int isDir = 0;
    int ownOutDir = 0;
    int ok;

    // Command line arguments learned from geeksforgeeks (resource 7)
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--dir") == 0) {
            if (i + 1 >= argc || dir || input) {
                fprintf(stderr, "Wrong argument after %s\n", argv[i]);
                return 1;
            }
            dir = argv[++i];
            isDir = 1;
        } else if (strcmp(argv[i], "-o") == 0 ||
                   strcmp(argv[i], "--output-dir") == 0) {
            if (i + 1 >= argc || outDir) {
                fprintf(stderr, "Wrong argument after %s\n", argv[i]);
                return 1;
            }
            outDir = argv[++i];
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return 1;
        } else if (!input && !dir) {
            input = argv[i];
        } else {
            fprintf(stderr, "Too many input arguments\n");
            return 1;
        }
    }

    if ((!input && !dir) || (input && dir)) {
        fprintf(stderr, "Use: calc [-d DIR] [-o OUTDIR] [input.txt]\n");
        return 1;
    }

    if (!outDir) {
        outDir = defaultDir(isDir ? dir : input, isDir);
        if (!outDir) {
            fprintf(stderr, "Cannot make default output directory name\n");
            return 1;
        }
        ownOutDir = 1;
    }

    if (!makeOutputDir(outDir)) {
        if (ownOutDir)
            free(outDir);
        return 1;
    }

    if (dir)
        ok = processDir(dir, outDir);
    else
        ok = calculateFile(input, outDir);

    free(answer);
    if (ownOutDir)
        free(outDir);
    return ok ? 0 : 1;
}
