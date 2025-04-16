#include <iostream>
#include <stack>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <regex>
#include <cmath>
#include <functional>

#define DEBUG

using namespace std;

stack<string> operators;
stack<string> temp_suffix_result;
unordered_map<string, double> variables; // Global container for variables

typedef struct symbol
{
    string name;
    string data;
    int typ;
    int pri;
    int pos;
    double val;
} SYMBOL;
void clear_stack(stack<string> &s, stack<string> &o)
{
    stack<string> empty;
    s.swap(empty);
    stack<string> empty2;
    o.swap(empty2);
}
/*
移除字符串中的空格
*/
string remove_spaces(string *str)
{
    str->erase(std::remove(str->begin(), str->end(), ' '), str->end());
    return *str;
}

/*
优先级map
*/
// 操作符优先级映射
std::unordered_map<std::string, int> operatorPrecedence = {
    {"&&", 4},
    {"||", 3},
    {"->", 2},
    {"<->", 1},

    {"+", 11},
    {"-", 11},
    {"*", 12},
    {"/", 12},
    {"**", 13},
    {"!", 14},
    {"==", 8},
    {"!=", 8},
    {"<", 9},
    {"<=", 9},
    {">", 9},
    {">=", 9}};

/*
函数名集合
*/
std::unordered_set<std::string> functions = {"max", "min", "log", "sin", "cos"};

/*
处理操作符
*/
// 处理操作符
void process_operator(const std::string &op, std::stack<std::string> &operators, std::stack<std::string> &temp_suffix_result)
{
    while (!operators.empty())
    {
        std::string topOp = operators.top();

        // Check if the top operator is a multi-character operator and find its precedence
        if (operatorPrecedence.find(topOp) != operatorPrecedence.end())
        {
            int topPrecedence = operatorPrecedence[topOp];
            int currentPrecedence = operatorPrecedence.at(op);

            // Compare based on precedence
            if (topOp != "(" && topPrecedence >= currentPrecedence)
            {
                temp_suffix_result.push(topOp);
                operators.pop();
            }
            else
            {
                break;
            }
        }
        else
        {
            break;
        }
    }
    operators.push(op);
}
/*
提示函数。用于提示用户的输入是否规范，或者是否有误。
参数表：
    oriStr: 原始字符串
    Message_level: 提示等级：
        1. "error"：错误提示
        2. "warning"：警告提示
        3. "info"：信息提示
    pos: 在原始字符串中错误/警告/提示的位置
    suggestion: 提示用户可能的替代方案
*/
void Hint(string oriStr, string Message_level, size_t pos, string suggestion = "")
{
    string level_output;
    string color_code;

    // 根据提示等级设置颜色和提示类型
    if (Message_level == "error")
    {
        level_output = "\033[1;31merror\033[0m"; // 红色
        color_code = "\033[1;31m";               // 红色
    }
    else if (Message_level == "warning")
    {
        level_output = "\033[1;33mwarning\033[0m"; // 黄色
        color_code = "\033[1;33m";                 // 黄色
    }
    else if (Message_level == "info")
    {
        level_output = "\033[1;36minfo\033[0m"; // 天蓝色 (青色)
        color_code = "\033[1;36m";              // 天蓝色 (青色)
    }

    // 输出提示类型
    cout << level_output << ": at position " << pos << endl;

    // 输出带颜色的原始字符串
    cout << "        " << oriStr.substr(0, pos);              // 错误前的部分，默认颜色
    cout << color_code << oriStr.substr(pos, 1) << "\033[0m"; // 错误字符，带颜色
    cout << oriStr.substr(pos + 1) << endl;                   // 错误后面的部分，默认颜色

    // 输出指向错误字符的波浪线，长度和错误字符保持一致
    cout << "        " << string(pos, ' ') << color_code << "^";
    cout << string(oriStr.length() - pos - 1, '~') << "\033[0m" << endl;

    // 输出建议替代方案
    if (!suggestion.empty())
    {
        if (Message_level == "error")
        {
            cout << "问题:  '\e[1;31m" << suggestion << "\e[0m'" << endl;
        }
        else if (Message_level == "info")
        {
            cout << "可选的建议: '\e[1;32m" << suggestion << "\e[0m'" << endl;
        }
        else if (Message_level == "warning")
        {
            cout << "警告: '\e[1;33m" << suggestion << "\e[0m'" << endl;
        }
    }

    // 如果是 error，终止程序
    if (Message_level == "error")
    {
        clear_stack(temp_suffix_result, operators);
        throw std::invalid_argument("本轮循环因异常而终止"); // 终止当前循环
    }
}

// 定义函数类型
using OperatorFunc = std::function<double(double, double)>;
using FunctionFunc = std::function<double(double)>;
using MultiFunctionFunc = std::function<double(const std::vector<double> &)>; // 定义 n 元函数类型

// 定义操作函数(在此添加函数后，记得去函数名map中添加对应名字)
double if_eq(double left, double right) { return left == right; }
double if_ne(double left, double right) { return left != right; }
double if_lt(double left, double right) { return left < right; }
double if_le(double left, double right) { return left <= right; }
double if_gt(double left, double right) { return left > right; }
double if_ge(double left, double right) { return left >= right; }
double Not(double left, double right) { return !right; };
double And(double left, double right) { return left && right; };
double Or(double left, double right) { return left || right; };
double implication(double left, double right) { return !left || right; };
double equivalence(double left, double right) { return (!left || right) && (left || !right); }

double add(double left, double right) { return left + right; }
double subtract(double left, double right) { return left - right; }
double multiply(double left, double right) { return left * right; }
double divide(double left, double right)
{
    if (right == 0)
    {
        clear_stack(temp_suffix_result, operators);
        throw std::invalid_argument("\033[1;31m除数不能为0\033[0m");
    }
    return left / right;
}
double power(double left, double right) { return std::pow(left, right); }
/* double max_func(double left, double right) { return std::max(left, right); }
double min_func(double left, double right) { return std::min(left, right); }
double log_func(double left, double right) {
    if (left == 1 || left <= 0 || right <= 0)
    {
        clear_stack(temp_suffix_result, operators);
        throw std::invalid_argument("\033[1;31m对数函数参数不合法\033[0m");
    }
     std::log(right) / std::log(left); } */
double sin_func(double x) { return std::sin(x); }
double cos_func(double x) { return std::cos(x); }
// 多元函数
double sum_func(const std::vector<double> &values)
{
    double sum = 0;
    for (double value : values)
    {
        sum += value;
    }
    return sum;
}
double avg_func(const std::vector<double> &values)
{
    double sum = 0;
    for (double value : values)
    {
        sum += value;
    }
    return sum / values.size();
}
// 定义各个多元函数
double max_func(const std::vector<double> &args)
{
    return *std::max_element(args.begin(), args.end());
}

double min_func(const std::vector<double> &args)
{
    return *std::min_element(args.begin(), args.end());
}

double log_func(const std::vector<double> &args)
{
    if (args.size() != 2)
    {
        throw std::invalid_argument("\033[1;31mlog 函数需要两个参数\033[0m");
    }
    double value = args[0];
    double base = args[1];
    if (base <= 0 || base == 1 || base <= 0)
    {
        throw std::invalid_argument("\033[1;31m非法的对数参数\033[0m");
    }
    return std::log(value) / std::log(base);
}

// 操作符映射表
std::unordered_map<std::string, OperatorFunc> operatorMap = {
    {"+", add},
    {"-", subtract},
    {"*", multiply},
    {"/", divide},
    {"**", power},
    {"==", if_eq},
    {"!=", if_ne},
    {"<", if_lt},
    {"<=", if_le},
    {">", if_gt},
    {">=", if_ge},
    {"!", Not},
    {"&&", And},
    {"||", Or},
    {"<->", equivalence},
    {"->", implication}};

// 一元函数映射表
std::unordered_map<std::string, FunctionFunc> functionMap = {
    {"sin", sin_func},
    {"cos", cos_func}};
// 多元函数映射表
std::unordered_map<std::string, MultiFunctionFunc> multiFunctionMap = {
    {"max", max_func},
    {"min", min_func},
    {"log", log_func},
    {"sum", sum_func},
    {"avg", avg_func},
};

// 多元函数执行器
double executeMultiFunction(const std::string &funcName, const std::vector<double> &args)
{
    auto it = multiFunctionMap.find(funcName);
    if (it != multiFunctionMap.end())
    {
        return it->second(args);
    }
    else
    {
        throw std::invalid_argument("未知的多元函数: " + funcName);
    }
}

// 操作符规范替换表
std::unordered_map<std::string, std::string> replacementMap = {

};

/*
优化原始表达式，方便转化为逆波兰表达式
*/
void Expression_optimization(string *str)
{
    // 遍历字符串，查找并替换大写的函数名为小写
    for (size_t i = 0; i < str->size(); ++i)
    {
        if (isalpha(str->at(i)))
        {
            size_t start = i;
            while (i < str->size() && isalpha(str->at(i)))
            {
                ++i;
            }
            string token = str->substr(start, i - start);
            string lower_token = token;
            transform(lower_token.begin(), lower_token.end(), lower_token.begin(), ::tolower);
            if (functions.find(lower_token) != functions.end())
            {
                // 只有在函数名包含大写字母时才调用 Hint
                if (token != lower_token)
                {
                    // 提示用户并指明位置
                    Hint(*str, "info", start, lower_token);
                    str->replace(start, token.size(), lower_token);
                }
            }
        }
    }

    // 使用替换表进行替换
    for (const auto &pair : replacementMap)
    {
        std::regex pattern(pair.first);
        std::string replacement = pair.second;
        std::smatch match;
        std::string temp_str = *str;
        while (std::regex_search(temp_str, match, pattern))
        {
            size_t pos = match.position();
            Hint(*str, "info", pos, replacement);
            *str = std::regex_replace(*str, pattern, replacement);
            temp_str = *str;
        }
    }

    // 检查并添加缺失的乘号
    for (size_t i = 0; i < str->size(); ++i)
    {
        if (isdigit(str->at(i)))
        {
            // 数字与括号间
            if (i + 1 < str->size() && str->at(i + 1) == '(')
            {
                str->insert(i + 1, "*");
                Hint(*str, "info", i + 1, "*");
            }
            // 数字与变量间或数字与函数间
            if (i + 1 < str->size() && isalpha(str->at(i + 1)))
            {
                str->insert(i + 1, "*");
                Hint(*str, "info", i + 1, "*");
            }
        }
        // 括号与变量间或括号与函数间
        if (str->at(i) == ')' && i + 1 < str->size() && (isalpha(str->at(i + 1)) || str->at(i + 1) == '('))
        {
            str->insert(i + 1, "*");
            Hint(*str, "info", i + 1, "*");
        }
        // 相反括号之间
        if (str->at(i) == ')' && i + 1 < str->size() && str->at(i + 1) == '(')
        {
            str->insert(i + 1, "*");
            Hint(*str, "info", i + 1, "*");
        }
    }

    // 在单独的负数，负号前面插入0
#ifdef DEBUG
    //cout << "ori_Expression_length: " << str->size() << endl;
#endif
    for (int i = 0; i < str->size(); i++)
    {
        if (str->at(i) == '-' && (i == 0 || str->at(i - 1) == '('))
        {
            str->insert(i, "0");
        }
    }
    for (int i = 0; i < str->size(); i++)
    {
        if (str->at(i) == '!')
        {
            str->insert(i, "1");
            i++;
        }
    } 

#ifdef DEBUG
    cout << "Expression_optimization: " << *str << endl;
#endif
}

// 获取操作符的最大长度
int get_max_operator_length(const std::unordered_map<std::string, int> &opMap)
{
    int maxLen = 0;
    for (const auto &pair : opMap)
    {
        maxLen = std::max(maxLen, static_cast<int>(pair.first.size()));
    }

    return maxLen;
}
int levenshteinDistance(const std::string &s1, const std::string &s2)
{
    const size_t len1 = s1.size(), len2 = s2.size();
    std::vector<std::vector<size_t>> d(len1 + 1, std::vector<size_t>(len2 + 1));

    for (size_t i = 0; i <= len1; ++i)
        d[i][0] = i;
    for (size_t i = 0; i <= len2; ++i)
        d[0][i] = i;

    for (size_t i = 1; i <= len1; ++i)
        for (size_t j = 1; j <= len2; ++j)
            d[i][j] = std::min({
                d[i - 1][j] + 1,                                   // 删除
                d[i][j - 1] + 1,                                   // 插入
                d[i - 1][j - 1] + (s1[i - 1] == s2[j - 1] ? 0 : 1) // 替换
            });

    return d[len1][len2];
}
std::string fuzzyMatchFunction(const std::string &current,
                               const std::unordered_map<std::string, FunctionFunc> &functionMap,
                               const std::unordered_map<std::string, MultiFunctionFunc> &multiFunctionMap)
{
    std::string bestMatch;
    int bestDistance = INT_MAX;

    // 遍历一元函数映射表
    for (const auto &pair : functionMap)
    {
        int distance = levenshteinDistance(current, pair.first);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestMatch = pair.first;
        }
    }

    // 遍历多元函数映射表
    for (const auto &pair : multiFunctionMap)
    {
        int distance = levenshteinDistance(current, pair.first);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestMatch = pair.first;
        }
    }

    return bestMatch;
}

void lexer(std::string *str)
{
    remove_spaces(str);
    int i = 0;
    bool lastWasOperatorOrOpenParenthesis = true; // 用于跟踪上一个字符是否为操作符或 '('

    // 获取最长操作符的长度
    int maxLen = get_max_operator_length(operatorPrecedence);
#ifdef DEBUG
    std::cout << "Max operator length: " << maxLen << std::endl;
#endif

    while (i < str->size())
    {
        char current = str->at(i);

        if (isdigit(current))
        { // 处理连续的数字
            std::string temp;
            while (i < str->size() && (isdigit(str->at(i)) || str->at(i) == '.'))
            {
                temp.push_back(str->at(i));
                i++;
            }
            temp_suffix_result.push(temp);
            lastWasOperatorOrOpenParenthesis = false;
        }
        else if (isalpha(current))
        { // 处理变量名或函数名
            std::string temp;
            while (i < str->size() && isalpha(str->at(i)))
            {
                temp.push_back(str->at(i));
                i++;
            }
            // 处理函数名或变量名
            if (functions.find(temp) != functions.end() || multiFunctionMap.find(temp) != multiFunctionMap.end())
            {
                operators.push(temp);
            }
            else
            {
                temp_suffix_result.push(temp);
            }
            lastWasOperatorOrOpenParenthesis = false;
        }
        else if (current == '(')
        {
            operators.push("(");
            lastWasOperatorOrOpenParenthesis = true;
            i++;
        }
        else if (current == ')')
        {
            while (!operators.empty() && operators.top() != "(")
            {
                temp_suffix_result.push(operators.top());
                operators.pop();
            }
            if (!operators.empty())
            {
                operators.pop(); // 移除开括号
            }
            if (!operators.empty() && functions.find(operators.top()) != functions.end())
            {
                temp_suffix_result.push(operators.top());
                operators.pop();
            }
            lastWasOperatorOrOpenParenthesis = false;
            i++;
        }
        else if (current == ',')
        { // 处理函数参数分隔符
            while (!operators.empty() && operators.top() != "(")
            {
                temp_suffix_result.push(operators.top());
                operators.pop();
            }
            i++;
        }
        else
        {
            // 处理操作符
            std::string op;
            bool foundOp = false;

            for (int len = maxLen; len >= 1; --len)
            {
                if (i + len <= str->size())
                {
                    op = str->substr(i, len);
                    if (operatorPrecedence.find(op) != operatorPrecedence.end())
                    {
                        foundOp = true;
                        break;
                    }
                }
            }

            if (foundOp)
            { // 处理操作符
                if (lastWasOperatorOrOpenParenthesis)
                {
                    if (op == "-")
                    {
                        temp_suffix_result.push("0"); // 在负号前加一个零
                    }
                    else if (op == "=" && op != "==")
                    {
                        clear_stack(temp_suffix_result, operators);
                        Hint(*str, "error", i, "赋值操作符 '=' 不能出现在这里");
                        return;
                    }
                    else
                    {
                        clear_stack(temp_suffix_result, operators);
                        Hint(*str, "error", i, "错误的操作符");
                        return;
                    }
                }
                process_operator(op, operators, temp_suffix_result);
                lastWasOperatorOrOpenParenthesis = false;
                i += op.length();
            }
            else
            { // 处理未知字符
                clear_stack(temp_suffix_result, operators);
                Hint(*str, "error", i, "未知的字符");
                return;
            }
        }
    }

    while (!operators.empty())
    {
        temp_suffix_result.push(operators.top());
        operators.pop();
    }
}

double Binary_Computing_Executor(double left, double right, const std::string op)
{
    auto it = operatorMap.find(op);
    if (it != operatorMap.end())
    {
        return it->second(left, right);
    }
    clear_stack(temp_suffix_result, operators);
    throw std::invalid_argument("未知的运算符: " + op);
}

double Unary_Computing_Executor(double value, const std::string func)
{
    auto it = functionMap.find(func);
    if (it != functionMap.end())
    {
        return it->second(value);
    }
    clear_stack(temp_suffix_result, operators);
    throw std::invalid_argument("未知的函数: " + func);
}
// 定义允许传入未定义变量的函数集合
std::unordered_set<std::string> neednt_args_func = {"func1", "func2"}; // 示例函数名

double calculate(string *str, stack<string> temp_suffix_result)
{
    std::stack<string> temp_resulet, temp_suffix;

    // 将 temp_suffix_result 逆序放入 temp_suffix 中
    while (!temp_suffix_result.empty())
    {
        temp_suffix.push(temp_suffix_result.top());
        temp_suffix_result.pop();
    }

    while (!temp_suffix.empty())
    {
        std::string current = temp_suffix.top();
        temp_suffix.pop();

        if (isdigit(current[0]))
        {
            temp_resulet.push(current);
        }
        
        else if (isalpha(current[0]))
        { // 处理变量使用
            if (variables.find(current) != variables.end())
            {
                temp_resulet.push(std::to_string(variables[current]));
            }
            else if (functionMap.find(current) != functionMap.end())
            {
                if (temp_resulet.size() < 1)
                {
                    Hint(*str, "error", str->find(current), "无效的表达式: 函数缺少参数");
                    clear_stack(temp_suffix_result, operators);
                    return 0; // 终止处理
                }
                double value = std::stod(temp_resulet.top());
                temp_resulet.pop();
                double result = Unary_Computing_Executor(value, current);
                temp_resulet.push(std::to_string(result));
            }
            else if (multiFunctionMap.find(current) != multiFunctionMap.end())
            {
                std::vector<double> args;

                // 从结果栈中提取所有参数到 args 中
                while (!temp_resulet.empty())
                {
                    args.push_back(std::stod(temp_resulet.top()));
                    temp_resulet.pop();
                }

                // 如果 args 为空，提示用户提供参数
                if (args.empty())
                {
                    Hint(current, "error", 0, "函数 '" + current + "' 缺少参数");
                }

                // 调用对应的多参数函数并将结果推送到结果栈
                double result = executeMultiFunction(current, args);
                temp_resulet.push(std::to_string(result));
            }
            else if (operatorMap.find(current) != operatorMap.end())
            {
                if (temp_resulet.size() < 2)
                {
                    Hint(*str, "error", str->find(current), "无效的表达式: 操作符缺少参数");
                    clear_stack(temp_suffix_result, operators);
                    return 0; // 终止处理
                }
                double right = std::stod(temp_resulet.top());
                temp_resulet.pop();
                double left = std::stod(temp_resulet.top());
                temp_resulet.pop();
                double result = Binary_Computing_Executor(left, right, current);
                temp_resulet.push(std::to_string(result));
            }
            else
            {
                std::string suggestedFunction = fuzzyMatchFunction(current, functionMap, multiFunctionMap);
                if (!suggestedFunction.empty())
                {
                    Hint(*str, "error", str->find(current), "变量 '" + current + "' 未定义. 你是否指的是 '" + suggestedFunction + "'?");
                }
                else
                {
                    Hint(*str, "error", str->find(current), "变量 '" + current + "' 未定义");
                }
                clear_stack(temp_suffix_result, operators);
                return 0; // 终止处理
            }
        }
        else if (current == "=")
        {
            // 处理赋值操作符
            if (temp_resulet.size() < 2)
            {
                Hint(*str, "error", str->find(current), "无效的表达式: 赋值缺少参数");
                clear_stack(temp_suffix_result, operators);
                return 0; // 终止处理
            }
            double right = std::stod(temp_resulet.top());
            temp_resulet.pop();
            std::string var_name = temp_resulet.top();
            temp_resulet.pop();

            if (!std::all_of(var_name.begin(), var_name.end(), ::isalpha))
            {
                Hint(*str, "error", str->find(current), "calculate报错：无效的变量名");
                clear_stack(temp_suffix_result, operators);
                return 0; // 终止处理
            }

            variables[var_name] = right;
            temp_resulet.push(std::to_string(right));
        }
        else
        {
            if (temp_resulet.size() < 2)
            {
                Hint(*str, "error", str->find(current), "无效的表达式: 缺少参数");
                clear_stack(temp_suffix_result, operators);
                return 0; // 终止处理
            }
            double right = std::stod(temp_resulet.top());
            temp_resulet.pop();
            double left = std::stod(temp_resulet.top());
            temp_resulet.pop();
            double result = Binary_Computing_Executor(left, right, current);
            temp_resulet.push(std::to_string(result));
        }
    }

    return std::stod(temp_resulet.top());
}

void create_variable(string var_name, string expression)
{
    if (isdigit(var_name[0]))
    {
        clear_stack(temp_suffix_result, operators);
        // throw std::invalid_argument("变量名不能以数字开头.");
    }

    if (functions.find(var_name) != functions.end())
    {
        clear_stack(temp_suffix_result, operators);
        Hint(var_name, "error", 0, "变量名不能与函数名重名");
        // throw std::invalid_argument("变量名不能与函数名重名.");
    }

    lexer(&expression);
    double result = calculate(&expression, temp_suffix_result);
    variables[var_name] = result;
    std::cout << var_name << " = " << result << std::endl;
}

void executer(string *str, SYMBOL *var)
{
    Expression_optimization(str);

    // 检查是否有赋值操作 '='
    size_t equal_pos = str->find('=');
    if (equal_pos != string::npos && (equal_pos == 0 || str->at(equal_pos - 1) != '<' && str->at(equal_pos - 1) != '>' && str->at(equal_pos - 1) != '!' && str->at(equal_pos + 1) != '='))
    {
        string var_name = str->substr(0, equal_pos);
        string expression = str->substr(equal_pos + 1);

        // 确保 '=' 被视为操作符
        if (var_name.empty() || !std::all_of(var_name.begin(), var_name.end(), ::isalpha))
        {
            Hint(*str, "error", equal_pos, "executer报错：无效的变量名");
            return;
        }

        // 处理变量赋值
        lexer(&expression);
        double result = calculate(&expression, temp_suffix_result);
        variables[var_name] = result;
        std::cout << var_name << " = " << result << std::endl;
    }
    else
    {
        lexer(str);
        double result = calculate(str, temp_suffix_result);
        std::cout << result << std::endl;
    }

    // 清空栈
    std::stack<string> empty;
    temp_suffix_result.swap(empty);
    std::stack<string> empty2;
    operators.swap(empty2);
}

#ifdef _WIN32
#include <Windows.h>
#endif // _WIN32
int main()
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif // _WIN32
    bool flag = 0;
    string input_str;
    //printf("Tiny_Pyhon 0.2 (tags/v0.2:hash, Sep. 13 2024, 19:50:41) [MSC v.1929 64 bit (AMD64)] on win32\nType \"help\", \"copyright\", \"credits\" or \"license\" for more information.\n");
    while (1)
    {
        clear_stack(temp_suffix_result, operators);
        if (!flag)
            cout << ">>> ";
        else
            flag = 0;
        getline(cin, input_str);
        if (input_str.empty())
        {
            flag = 1;
            cout << ">>> ";
            continue;
        }
        if (input_str == "exit")
            break;
        SYMBOL var;
        try
        {
            executer(&input_str, &var);
        }
        catch (const std::invalid_argument &e)
        {
            clear_stack(temp_suffix_result, operators);
            std::cerr << e.what() << std::endl;
        }
    }

    return 0;
}