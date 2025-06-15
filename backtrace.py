import re
import sys

# 从控制台输出的文本中提取backtrace地址
def extract_backtrace(console_text):
    backtrace_pattern = r'backtrace.*?:\n(.*?)\nend of backtrace.*?:'
    match = re.search(backtrace_pattern, console_text, re.DOTALL)
    if match:
        return [line.strip() for line in match.group(1).split('\n') if line.strip()]
    return []

# 根据地址在asm文件中查找最近的函数名
def find_function_name(asm_file, address):
    address = address.lower().lstrip('0x')
    closest_function = None
    function_pattern = re.compile(r'<(.*?)>:')
    with open(asm_file, 'r') as file:
        lines = file.readlines()

    for i, line in enumerate(lines):
        if address in line:
            for back_line in reversed(lines[:i]):
                match = function_pattern.search(back_line)
                if match:
                    closest_function = match.group(1)
                    return closest_function
    return "Function not found"

# 主程序
def main(console_log_path, asm_file):
    with open(console_log_path, 'r') as f:
        console_text = f.read()

    addresses = extract_backtrace(console_text)
    result = {}

    for addr in addresses:
        func_name = find_function_name(asm_file, addr)
        result[addr] = func_name

    for addr, func in result.items():
        print(f"{addr} ----> {func}")

# 入口
if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f"用法: python3 {sys.argv[0]} <output.log路径> <kernel.asm路径>")
        sys.exit(1)

    console_log_path = sys.argv[1]
    asm_filepath = sys.argv[2]
    main(console_log_path, asm_filepath)
