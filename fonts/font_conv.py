import sys
import os
import struct
from PIL import Image, ImageFont, ImageDraw

def generate_font_bin(ttf_path, size, output_path):
    """
    将 TTF 矢量字体转换为 1-bit 点阵二进制资产。
    """
    print(f"正在转换 {ttf_path} (大小 {size}) -> {output_path}...")
    
    if not os.path.exists(ttf_path):
        print(f"错误：找不到字体文件 {ttf_path}")
        return
        
    try:
        font = ImageFont.truetype(ttf_path, size)
    except Exception as e:
        print(f"载入字体失败: {e}")
        return

    # 字符集：ASCII 32 (空格) 到 126 (~)
    chars = [chr(i) for i in range(32, 127)]
    char_data = []
    offsets = []
    
    # 使用固定高度以简化 C 端的 Bit-Blit 逻辑
    font_height = size
    
    # 计算头部尺寸：Magic(4) + Height(2) + Count(2) + Offsets(N*4)
    header_size = 8 + (len(chars) * 4)
    current_offset = 0

    for char in chars:
        # 获取字符的边界框以计算宽度
        bbox = font.getbbox(char)
        if not bbox: # 空格或空字符
            width = size // 3
            img = Image.new('1', (width, font_height), 0)
        else:
            # bbox 为 (left, top, right, bottom)
            width = bbox[2] - bbox[0]
            if width <= 0: width = size // 4
            
            img = Image.new('1', (width, font_height), 0)
            draw = ImageDraw.Draw(img)
            # 绘制字符。使用 -bbox[0] 偏置使其紧贴位图左边缘。
            draw.text((-bbox[0], 0), char, font=font, fill=1)

        # 转换为位流（ packed bits，水平扫描，高位在前）
        bits = []
        for y in range(font_height):
            for x in range(width):
                bits.append(1 if img.getpixel((x, y)) > 0 else 0)
        
        packed_bytes = bytearray()
        for i in range(0, len(bits), 8):
            byte = 0
            for j in range(8):
                if i + j < len(bits):
                    if bits[i + j]:
                        byte |= (0x80 >> j)
            packed_bytes.append(byte)
            
        char_entry = {
            'width': width,
            'data': packed_bytes
        }
        char_data.append(char_entry)
        offsets.append(header_size + current_offset)
        current_offset += 1 + len(packed_bytes) # 1字节宽度 + 数据区

    # 确保存储目录存在
    os.makedirs(os.path.dirname(output_path), exist_ok=True)

    with open(output_path, 'wb') as f:
        # 幻数：'FONT'
        f.write(b'FONT') 
        f.write(struct.pack('<H', font_height))
        f.write(struct.pack('<H', len(chars)))
        
        # 偏移表（文件内的绝对偏移）
        for off in offsets:
            f.write(struct.pack('<I', off))
            
        # 字符数据块
        for entry in char_data:
            f.write(struct.pack('B', entry['width']))
            f.write(entry['data'])

    print(f"成功。资产已写入 {output_path} ({os.path.getsize(output_path)} 字节)。")

if __name__ == "__main__":
    # 示例用法: python font_conv.py Inter_24pt-Bold.ttf 24 font_24px.bin
    if len(sys.argv) < 4:
        print("用法: python font_conv.py <ttf源文件path> <字号size> <输出bin文件path>")
    else:
        generate_font_bin(sys.argv[1], int(sys.argv[2]), sys.argv[3])
