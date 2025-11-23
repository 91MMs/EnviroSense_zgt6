import os
import glob
import xml.etree.ElementTree as ET

# ================= 配置区域 =================
# Keil 工程文件夹名称
MDK_DIR_NAME = "MDK-ARM"

# Keil 编译器自带的标准库路径 (保持绝对路径)
# 如果换了电脑，只需要修改这里即可，修改为新的 Keil 安装路径
KEIL_SYSTEM_INCLUDES = [
    r"F:\MDK5\ARM\ARMCC\include",
    r"F:\MDK5\ARM\ARMCC\include\rw"
]

# 基础 Flags
BASE_FLAGS = [
    "-xc",
    "-std=c99",
    "--target=arm-none-eabi",
    "-fms-extensions",
    "-fdeclspec",
    "-D__forceinline=inline",
    "-D__irq=",
    "-D__value_in_regs=",
    "-D__weak=__attribute__((weak))",
    "-D__packed=__attribute__((__packed__))",
    "-D__align(x)=",
    "-D__int64=long long",
    "-D__svc(x)=",
    "-D__declspec(x)=",
    "-D__asm(x)=", 
    "-D__inline=inline"
]
# ===========================================

def find_keil_project():
    """
    自动在 MDK-ARM 目录下寻找 .uvprojx 文件
    """
    search_pattern = os.path.join(MDK_DIR_NAME, "*.uvprojx")
    files = glob.glob(search_pattern)
    
    if not files:
        print(f"❌ 错误: 在 {MDK_DIR_NAME} 目录下找不到任何 .uvprojx 文件！")
        return None
    
    if len(files) > 1:
        print(f"⚠️ 警告: 找到多个工程文件: {files}，默认使用第一个: {files[0]}")
    
    print(f"🔎 自动锁定工程文件: {files[0]}")
    return files[0]

def parse_keil_project(project_path):
    try:
        tree = ET.parse(project_path)
        root = tree.getroot()
        
        includes = []
        defines = []
        
        for target in root.findall(".//Target"):
            cads = target.find(".//Cads")
            if cads is None: continue
                
            inc_path_elem = cads.find(".//IncludePath")
            if inc_path_elem is not None and inc_path_elem.text:
                paths = inc_path_elem.text.split(';')
                includes.extend([p.strip() for p in paths if p.strip()])

            define_elem = cads.find(".//Define")
            if define_elem is not None and define_elem.text:
                defs = define_elem.text.split(',')
                for d in defs:
                    defines.extend([x.strip() for x in d.split(' ') if x.strip()])
                    
        return includes, defines

    except Exception as e:
        print(f"❌ 解析 XML 出错: {e}")
        return None, None

def generate_flags(includes, defines, project_path):
    content = []
    content.extend(BASE_FLAGS)
    
    # 写入宏定义
    unique_defines = sorted(list(set(defines)))
    for d in unique_defines:
        content.append(f"-D{d}")

    # 写入 Keil 标准库路径 (绝对路径)
    for sys_inc in KEIL_SYSTEM_INCLUDES:
        clean_path = sys_inc.replace("\\", "/")
        content.append(f"-I{clean_path}")
        
    # 获取目录信息
    root_dir = os.getcwd()
    keil_dir = os.path.dirname(os.path.abspath(project_path))
    
    # 写入项目头文件路径
    unique_includes = sorted(list(set(includes)))
    for inc in unique_includes:
        # 1. 还原为绝对路径
        if os.path.isabs(inc):
            abs_path = inc
        else:
            abs_path = os.path.abspath(os.path.join(keil_dir, inc))
            
        # 2. 尝试转相对路径
        try:
            rel_path = os.path.relpath(abs_path, root_dir)
            if rel_path.startswith("..") and not abs_path.startswith(root_dir):
                final_path = abs_path
            else:
                final_path = rel_path
        except ValueError:
            final_path = abs_path

        # 3. 格式化
        final_path = final_path.replace("\\", "/")
        if final_path.startswith("./"):
            final_path = final_path[2:]
            
        content.append(f"-I{final_path}")
        
    return content

def main():
    # 1. 自动寻找工程文件
    project_path = find_keil_project()
    if not project_path:
        return

    # 2. 解析
    includes, defines = parse_keil_project(project_path)
    if includes is None: 
        return
    
    # 3. 生成配置
    flags = generate_flags(includes, defines, project_path)
    
    # 4. 写入文件
    with open("compile_flags.txt", "w", encoding="utf-8") as f:
        f.write("\n".join(flags))
        
    print(f"✅ 成功！已生成 {len(flags)} 条配置。")

if __name__ == "__main__":
    main()