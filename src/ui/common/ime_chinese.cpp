#include "ime_chinese.h"
#include <cstring>
#include <cstdio>

// ============================================================
//  T9 数字键 → 候选拼音字母表
//  标准 T9 映射（0和1不对应字母）
// ============================================================
static const char* T9_CN_LETTERS[10] = {
    "",     // 0: 无（中文模式下0键不用）
    "",     // 1: 无
    "abc",  // 2
    "def",  // 3
    "ghi",  // 4
    "jkl",  // 5
    "mno",  // 6
    "pqrs", // 7
    "tuv",  // 8
    "wxyz"  // 9
};

// ============================================================
//  拼音 → 汉字静态词典
//  覆盖 GB2312 一级汉字（3755字）中最常用的部分
//  重点加强：姓名用字、日常用字
//  每条记录: { 拼音, 候选汉字串（每个汉字3字节UTF-8） }
// ============================================================
struct PinyinEntry {
    const char* pinyin;
    const char* chars;  // 按使用频率从高到低排列的汉字，UTF-8编码
};

static const PinyinEntry PINYIN_DICT[] = {
    {"a",      "啊阿哦呵"},
    {"ai",     "爱艾哎唉挨矮"},
    {"an",     "安案按暗岸"},
    {"ang",    "昂"},
    {"ao",     "奥澳傲熬"},
    {"ba",     "把爸吧巴罢拔八百"},
    {"bai",    "白百败拜摆"},
    {"ban",    "班办半板版般搬伴扮"},
    {"bang",   "帮棒榜磅旁傍"},
    {"bao",    "保报包宝抱暴爆薄博"},
    {"bei",    "被北背杯备悲辈倍贝"},
    {"ben",    "本奔笨"},
    {"beng",   "崩蹦泵"},
    {"bi",     "比必笔闭避鼻壁彼毕碧"},
    {"bian",   "变边便遍编辨辩"},
    {"biao",   "表标彪"},
    {"bie",    "别鳖"},
    {"bin",    "宾彬斌滨濒"},
    {"bing",   "病并冰兵饼"},
    {"bo",     "波博播脖伯泊帛勃"},
    {"bu",     "不部步布补捕哺"},
    {"ca",     "擦"},
    {"cai",    "才采彩菜财踩猜"},
    {"can",    "参残餐灿惨"},
    {"cang",   "藏仓苍"},
    {"cao",    "曹操草"},
    {"ce",     "测册侧策"},
    {"ceng",   "曾层"},
    {"cha",    "查差察插茶叉"},
    {"chai",   "柴拆钗"},
    {"chan",   "产禅缠铲颤"},
    {"chang",  "长场常昌唱厂畅倡"},
    {"chao",   "朝超潮抄"},
    {"che",    "车撤彻"},
    {"chen",   "陈晨沉臣趁衬尘"},
    {"cheng",  "成程城诚承称乘"},
    {"chi",    "吃持迟齿斥尺"},
    {"chong",  "重冲崇虫充"},
    {"chou",   "愁丑臭抽"},
    {"chu",    "出处初除储楚触"},
    {"chuan",  "传船穿川串"},
    {"chuang", "创窗床闯"},
    {"chui",   "吹垂锤"},
    {"chun",   "春纯淳醇"},
    {"ci",     "此次词刺磁慈辞"},
    {"cong",   "从聪丛"},
    {"cu",     "促粗醋簇"},
    {"cuan",   "蹿窜"},
    {"cui",    "崔催翠脆萃"},
    {"cun",    "村存寸"},
    {"cuo",    "错挫措搓"},
    {"da",     "打大达答搭"},
    {"dai",    "代带待戴袋贷呆"},
    {"dan",    "单但担蛋旦弹"},
    {"dang",   "当党挡荡"},
    {"dao",    "道到导倒刀捣"},
    {"de",     "的得德地"},
    {"deng",   "等灯登邓凳瞪"},
    {"di",     "地第敌底递帝滴"},
    {"dian",   "电点店典顿甸殿"},
    {"diao",   "调雕吊钓"},
    {"die",    "跌叠蝶"},
    {"ding",   "定顶丁订钉"},
    {"dong",   "动东懂栋冬董"},
    {"dou",    "都斗豆逗"},
    {"du",     "度读独都肚渡督"},
    {"duan",   "断段短端"},
    {"dui",    "对队堆兑"},
    {"dun",    "顿盾蹲"},
    {"duo",    "多夺朵躲"},
    {"e",      "鹅额俄恶饿"},
    {"en",     "恩"},
    {"er",     "二而耳儿"},
    {"fa",     "发法罚乏伐"},
    {"fan",    "反饭范繁番凡烦犯"},
    {"fang",   "方放房防访仿芳"},
    {"fei",    "非飞费废肥"},
    {"fen",    "分粉份纷奋愤"},
    {"feng",   "风丰封峰奉凤缝"},
    {"fo",     "佛"},
    {"fu",     "服福父副夫府付富负"},
    {"ga",     "噶"},
    {"gai",    "该改盖概"},
    {"gan",    "感干敢赶甘肝杆"},
    {"gang",   "刚港钢纲岗"},
    {"gao",    "高告搞稿"},
    {"ge",     "个各格歌革隔葛"},
    {"gei",    "给"},
    {"gen",    "根跟"},
    {"geng",   "更耕"},
    {"gong",   "工公共功宫攻供贡"},
    {"gou",    "够购构狗沟"},
    {"gu",     "古故顾股谷鼓固骨"},
    {"gua",    "刮挂括瓜"},
    {"guai",   "乖怪拐"},
    {"guan",   "关管观官惯贯冠"},
    {"guang",  "广光逛"},
    {"gui",    "贵规归鬼"},
    {"gun",    "滚棍"},
    {"guo",    "国果过锅"},
    {"ha",     "哈"},
    {"hai",    "还海害孩骇"},
    {"han",    "汉含喊旱寒"},
    {"hang",   "航行杭"},
    {"hao",    "好号浩毫豪"},
    {"he",     "和合何河喝核荷"},
    {"hei",    "黑"},
    {"hen",    "很恨"},
    {"heng",   "衡横亨"},
    {"hong",   "红洪宏轰虹"},
    {"hou",    "后候厚猴"},
    {"hu",     "互护湖虎呼户胡"},
    {"hua",    "化画花华话划"},
    {"huai",   "坏怀淮"},
    {"huan",   "换欢环幻还"},
    {"huang",  "黄皇荒慌"},
    {"hui",    "会回汇惠慧"},
    {"hun",    "婚混魂"},
    {"huo",    "活火或获货"},
    {"ji",     "机基积极记技既继急"},
    {"jia",    "家加价假甲嘉佳"},
    {"jian",   "见建简件键间剑减"},
    {"jiang",  "江将讲强降奖疆"},
    {"jiao",   "交教觉脚叫较"},
    {"jie",    "解接结节界姐杰"},
    {"jin",    "进金今近仅尽"},
    {"jing",   "经精境静敬景竟"},
    {"jiong",  "窘"},
    {"jiu",    "就九旧救纠久"},
    {"ju",     "局举句拒具距聚据"},
    {"juan",   "捐卷娟"},
    {"jue",    "觉决绝角"},
    {"jun",    "军均俊君"},
    {"ka",     "卡咖"},
    {"kai",    "开凯慨"},
    {"kan",    "看刊"},
    {"kang",   "康抗"},
    {"kao",    "考靠"},
    {"ke",     "可科客刻克课"},
    {"ken",    "肯"},
    {"kong",   "控空孔"},
    {"kou",    "口扣"},
    {"ku",     "苦哭库"},
    {"kua",    "夸跨跨"},
    {"kuai",   "快块筷"},
    {"kuan",   "宽款"},
    {"kuang",  "况矿框狂"},
    {"kui",    "亏愧"},
    {"kun",    "困坤"},
    {"kuo",    "扩括阔"},
    {"la",     "拉啦辣蜡"},
    {"lai",    "来赖"},
    {"lan",    "蓝览懒栏"},
    {"lang",   "浪郎朗"},
    {"lao",    "老劳捞"},
    {"le",     "了乐勒"},
    {"lei",    "类累泪"},
    {"li",     "力里利历理李立例厘离"},
    {"lian",   "连联练廉怜恋"},
    {"liang",  "两量亮良梁粮"},
    {"liao",   "了聊料廖"},
    {"lie",    "列烈裂"},
    {"lin",    "林临邻凌令"},
    {"ling",   "领令灵零"},
    {"liu",    "六留流刘柳"},
    {"long",   "龙隆弄笼"},
    {"lou",    "楼露漏"},
    {"lu",     "路陆绿鲁录炉"},
    {"lv",     "律绿旅虑吕"},
    {"luan",   "乱卵"},
    {"lun",    "论轮伦"},
    {"luo",    "落络罗裸"},
    {"ma",     "马妈吗麻码"},
    {"mai",    "买卖迈麦"},
    {"man",    "满慢漫蛮"},
    {"mang",   "忙茫盲"},
    {"mao",    "毛猫帽矛贸"},
    {"mei",    "没美妹每煤"},
    {"men",    "们门"},
    {"meng",   "梦猛蒙盟"},
    {"mi",     "米密迷秘眯"},
    {"mian",   "面棉眠免"},
    {"miao",   "苗妙庙描"},
    {"min",    "民敏闽"},
    {"ming",   "明名命鸣铭"},
    {"mo",     "末模磨默摸"},
    {"mou",    "某谋"},
    {"mu",     "目木母墓幕"},
    {"na",     "那拿哪纳"},
    {"nai",    "奶耐乃"},
    {"nan",    "南男难"},
    {"nao",    "脑闹"},
    {"nei",    "内"},
    {"nen",    "嫩"},
    {"neng",   "能"},
    {"ni",     "你尼逆拟泥"},
    {"nian",   "年念"},
    {"niang",  "娘酿"},
    {"niao",   "鸟尿"},
    {"ning",   "宁凝"},
    {"niu",    "牛纽"},
    {"nong",   "农弄浓"},
    {"nu",     "努怒奴"},
    {"nv",     "女"},
    {"nuan",   "暖"},
    {"nuo",    "挪诺"},
    {"o",      "哦噢"},
    {"ou",     "偶欧"},
    {"pa",     "怕爬派"},
    {"pai",    "排派拍牌"},
    {"pan",    "盘判攀潘"},
    {"pang",   "旁胖庞"},
    {"pao",    "跑炮抛"},
    {"pei",    "配培陪佩裴"},
    {"pen",    "盆喷"},
    {"peng",   "碰朋彭澎蓬"},
    {"pi",     "批皮疲匹辟譬"},
    {"pian",   "片偏骗"},
    {"piao",   "漂票"},
    {"pin",    "品频拼贫"},
    {"ping",   "平评屏瓶"},
    {"po",     "破迫泊婆坡"},
    {"pu",     "普铺朴扑"},
    {"qi",     "起其气期企器齐奇棋"},
    {"qia",    "洽"},
    {"qian",   "前钱千迁签浅"},
    {"qiang",  "强抢墙"},
    {"qiao",   "桥巧瞧悄"},
    {"qie",    "切且"},
    {"qin",    "亲琴勤"},
    {"qing",   "请情清庆轻青"},
    {"qiong",  "穷"},
    {"qiu",    "求球秋"},
    {"qu",     "取去区趋曲渠"},
    {"quan",   "全权拳泉"},
    {"que",    "确缺却"},
    {"qun",    "群裙"},
    {"ran",    "然染"},
    {"rang",   "让"},
    {"rao",    "绕"},
    {"re",     "热"},
    {"ren",    "人任认仁韧"},
    {"rong",   "容荣融"},
    {"rou",    "肉柔"},
    {"ru",     "如入儒"},
    {"ruan",   "软"},
    {"run",    "润"},
    {"ruo",    "弱若"},
    {"sa",     "撒"},
    {"sai",    "赛塞"},
    {"san",    "三散参"},
    {"sang",   "丧"},
    {"sao",    "扫嫂"},
    {"se",     "色"},
    {"sen",    "森"},
    {"sha",    "沙杀傻"},
    {"shan",   "山善闪删"},
    {"shang",  "上商赏伤"},
    {"shao",   "少绍邵"},
    {"she",    "社设射摄"},
    {"shen",   "深身神申慎审"},
    {"sheng",  "生省声升胜剩"},
    {"shi",    "是时事世市史实试失式使"},
    {"shou",   "收手守受寿首"},
    {"shu",    "书数树述熟输"},
    {"shua",   "刷耍"},
    {"shuai",  "帅摔"},
    {"shuan",  "拴"},
    {"shuang", "双爽"},
    {"shui",   "水谁税睡"},
    {"shun",   "顺瞬"},
    {"shuo",   "说"},
    {"si",     "四思死似私寺"},
    {"song",   "送宋松"},
    {"su",     "素速俗苏"},
    {"suan",   "算酸"},
    {"sui",    "虽随岁碎"},
    {"sun",    "孙损"},
    {"suo",    "所索缩"},
    {"ta",     "他她它踏塌"},
    {"tai",    "太台抬泰态"},
    {"tan",    "谈探弹坦叹贪"},
    {"tang",   "唐糖堂汤"},
    {"tao",    "套逃桃陶掏"},
    {"te",     "特"},
    {"teng",   "腾疼"},
    {"ti",     "题提体替踢"},
    {"tian",   "天田甜填"},
    {"tiao",   "条跳调挑"},
    {"tie",    "铁贴"},
    {"ting",   "听厅停"},
    {"tong",   "同通童痛统"},
    {"tou",    "头透投偷"},
    {"tu",     "图土突兔"},
    {"tuan",   "团"},
    {"tui",    "推腿退"},
    {"tun",    "吞屯"},
    {"tuo",    "脱拖托"},
    {"wa",     "瓦挖"},
    {"wai",    "外歪"},
    {"wan",    "完万晚湾玩"},
    {"wang",   "王往望忘网"},
    {"wei",    "为位未味威微围"},
    {"wen",    "文问稳温"},
    {"weng",   "翁"},
    {"wo",     "我握卧窝"},
    {"wu",     "无五物务误吴"},
    {"xi",     "系西习席希喜细析"},
    {"xia",    "下夏峡"},
    {"xian",   "先现限线县献显"},
    {"xiang",  "想象向项乡香详"},
    {"xiao",   "小效校消晓笑"},
    {"xie",    "写谢协些鞋"},
    {"xin",    "心新信辛"},
    {"xing",   "行形星性兴醒"},
    {"xiong",  "雄兄熊"},
    {"xiu",    "修秀休"},
    {"xu",     "需许续叙绪"},
    {"xuan",   "选宣旋玄"},
    {"xue",    "学雪"},
    {"xun",    "询寻训迅"},
    {"ya",     "牙压亚雅"},
    {"yan",    "言演研验眼颜延严"},
    {"yang",   "样阳洋杨央"},
    {"yao",    "要药摇遥"},
    {"ye",     "业也夜页野"},
    {"yi",     "一以已意义易宜益艺"},
    {"yin",    "因引印阴音银隐"},
    {"ying",   "应英营影迎赢"},
    {"yong",   "用永勇涌"},
    {"you",    "有又由友游优"},
    {"yu",     "与于语余育遇玉预"},
    {"yuan",   "原远元院园员援"},
    {"yue",    "月约越跃悦"},
    {"yun",    "云运允匀"},
    {"za",     "杂"},
    {"zai",    "在再载"},
    {"zan",    "赞暂"},
    {"zang",   "藏脏"},
    {"zao",    "早造找"},
    {"ze",     "则责择"},
    {"zeng",   "增曾"},
    {"zha",    "扎炸"},
    {"zhai",   "宅摘"},
    {"zhan",   "站战展占"},
    {"zhang",  "张章长掌"},
    {"zhao",   "照找召赵"},
    {"zhe",    "这者折"},
    {"zhei",   "这"},
    {"zhen",   "真针振珍镇"},
    {"zheng",  "正政整争证"},
    {"zhi",    "之知制治职直值"},
    {"zhong",  "中种重众终"},
    {"zhou",   "周州洲舟"},
    {"zhu",    "主注住著助朱"},
    {"zhua",   "抓"},
    {"zhuai",  "拽"},
    {"zhuan",  "专转"},
    {"zhuang", "装壮庄"},
    {"zhui",   "追"},
    {"zhun",   "准"},
    {"zhuo",   "卓着桌"},
    {"zi",     "子自字资紫"},
    {"zong",   "总综宗"},
    {"zou",    "走奏"},
    {"zu",     "组族足阻"},
    {"zuan",   "钻"},
    {"zui",    "最嘴罪"},
    {"zun",    "尊遵"},
    {"zuo",    "做作左座坐"},
};

static const int PINYIN_DICT_SIZE = (int)(sizeof(PINYIN_DICT) / sizeof(PINYIN_DICT[0]));

// ============================================================
//  拼音缓冲区状态
// ============================================================

// 存储用户已按下的数字序列（最多 IME_CN_MAX_PINYIN_LEN 个）
static int  g_digit_buf[IME_CN_MAX_PINYIN_LEN];
static int  g_digit_len = 0;

// ============================================================
//  内部工具函数
// ============================================================

/**
 * @brief 递归枚举数字序列对应的所有拼音前缀组合
 *        例如: 数字 [4,3] → letters[4]='ghi', letters[3]='def'
 *        → 枚举出 "gd","ge","gf","hd","he","hf","id","ie","if" 共9种
 *        匹配词典中有效的拼音前缀（做前缀匹配）
 *
 * @param digits    数字序列
 * @param n         数字序列长度
 * @param buf       当前已拼出的字符串
 * @param depth     当前递归深度（已处理到第几个数字）
 * @param results   输出：所有有效拼音前缀对应的候选汉字（最多 IME_CN_MAX_CANDIDATES 个）
 */
static void enumerate_pinyin(
    const int* digits, int n,
    char* buf, int depth,
    std::vector<std::string>& results
) {
    if ((int)results.size() >= IME_CN_MAX_CANDIDATES) return;

    if (depth == n) {
        // 已经拼完了所有数字，做前缀匹配
        // buf 里是当前拼出的字母串（如 "li"）
        int buf_len = (int)strlen(buf);
        for (int i = 0; i < PINYIN_DICT_SIZE; i++) {
            if (strncmp(PINYIN_DICT[i].pinyin, buf, buf_len) == 0) {
                // 找到前缀匹配的拼音，取其候选汉字
                const char* chars = PINYIN_DICT[i].chars;
                int char_len = (int)strlen(chars);
                // 每个汉字 UTF-8 占 3 字节
                for (int j = 0; j + 3 <= char_len; j += 3) {
                    if ((int)results.size() >= IME_CN_MAX_CANDIDATES) return;
                    results.push_back(std::string(chars + j, 3));
                }
            }
        }
        return;
    }

    // 继续递归枚举下一个数字对应的每个字母
    int d = digits[depth];
    if (d < 0 || d > 9) return;
    const char* letters = T9_CN_LETTERS[d];
    int letter_count = (int)strlen(letters);

    int cur_pos = (int)strlen(buf);
    for (int k = 0; k < letter_count; k++) {
        buf[cur_pos] = letters[k];
        buf[cur_pos + 1] = '\0';
        enumerate_pinyin(digits, n, buf, depth + 1, results);
        if ((int)results.size() >= IME_CN_MAX_CANDIDATES) return;
    }
    buf[cur_pos] = '\0'; // 回溯
}

// ============================================================
//  公开 API 实现
// ============================================================

void ime_cn_push_digit(int digit) {
    if (g_digit_len >= IME_CN_MAX_PINYIN_LEN) return;
    if (digit < 0 || digit > 9) return;
    g_digit_buf[g_digit_len++] = digit;
}

void ime_cn_pop() {
    if (g_digit_len > 0) {
        g_digit_len--;
    }
}

void ime_cn_reset() {
    g_digit_len = 0;
}

std::vector<std::string> ime_cn_get_candidates() {
    std::vector<std::string> results;
    if (g_digit_len == 0) return results;

    char buf[IME_CN_MAX_PINYIN_LEN + 2] = {0};
    enumerate_pinyin(g_digit_buf, g_digit_len, buf, 0, results);
    return results;
}

std::string ime_cn_get_pinyin_display() {
    if (g_digit_len == 0) return "";
    // 返回数字序列对应每个键位第一个字母的组合作为提示
    // 例如 [4,3] → "gd"
    std::string s;
    for (int i = 0; i < g_digit_len; i++) {
        int d = g_digit_buf[i];
        if (d >= 2 && d <= 9) {
            s += T9_CN_LETTERS[d][0];
        }
    }
    return s;
}
