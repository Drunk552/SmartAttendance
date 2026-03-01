import os
import re

# ================= 配置区 =================
# 指定扫描 ./src/ui下文件
PROJECT_DIR = './src/ui'  
FILE_EXTENSIONS = ('.cpp', '.h', '.c') 
# 如果里面还有不想扫的文件夹，可以写在这里
EXCLUDE_DIRS = ['font_noto_16.c'] 
# ==========================================

pattern = re.compile(r'"([^"\n\r]*[\u4e00-\u9fa5]+[^"\n\r]*)"')

# 这个集合现在用来装“单个字符”，而不是“整句话”
unique_chars = set()

for root, dirs, files in os.walk(PROJECT_DIR):
    # 排除不需要的文件夹
    dirs[:] = [d for d in dirs if d not in EXCLUDE_DIRS]

    for file in files:
        if file.endswith(FILE_EXTENSIONS):
            file_path = os.path.join(root, file)
            try:
                with open(file_path, 'r', encoding='utf-8') as f:
                    # 逐行读取
                    for line in f:
                        matches = pattern.findall(line)
                        for match in matches:
                            # 把找到的句子打散成单个字符
                            for char in match:
                                # 过滤掉空格和英文字母/数字（因为字库工具通常自带英文字母）
                                # 如果你连特殊符号和英文也要一起提取，可以把下面这个 if 判断去掉，直接 unique_chars.add(char)
                                if not char.isspace() and not char.isascii():
                                    unique_chars.add(char)
            except Exception as e:
                pass 

# 将结果输出到文件，这次所有的字会连成完整的一行，方便直接复制到字库生成工具里
with open('chinese_strings.txt', 'w', encoding='utf-8-sig') as f:
    # 将集合中的单字拼接成一个长字符串
    final_string = "".join(sorted(unique_chars))
    f.write(final_string)

print(f"提取完成！你的 UI 工程共使用了 {len(unique_chars)} 个不重复的中文字符。")
print("已将这串连续的单字保存到 chinese_strings.txt，可以直接用来生成 LVGL 字库了！")