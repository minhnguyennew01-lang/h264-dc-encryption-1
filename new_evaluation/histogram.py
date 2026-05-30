import cv2
import matplotlib.pyplot as plt
import numpy as np

# Load 2 ảnh
image1 = cv2.imread('/home/minh/Documents/NCKH20262/bitstream_impl_arnold2d/iframe_5.png')
image2 = cv2.imread('/home/minh/Documents/NCKH20262/bitstream_impl_arnold2d/iframe_5_s1_encrypted.png')



# Kiểm tra xem ảnh có load được không
if image1 is None or image2 is None:
    print("Lỗi: Không thể load ảnh. Kiểm tra đường dẫn file.")
    exit()

# OpenCV load ảnh ở định dạng BGR, chuyển sang RGB để hiển thị đúng màu
image1_rgb = cv2.cvtColor(image1, cv2.COLOR_BGR2RGB)
image2_rgb = cv2.cvtColor(image2, cv2.COLOR_BGR2RGB)

# Tạo figure với 4 hàng, 2 cột (ảnh gốc + histogram từng kênh)
fig, axes = plt.subplots(4, 2, figsize=(14, 14))

# Màu sắc cho các kênh
colors = ('red', 'green', 'blue')
channel_names = ('R', 'G', 'B')

# Vẽ ảnh gốc ở hàng đầu
axes[0, 0].imshow(image1_rgb)
axes[0, 0].set_title('Ảnh 1', fontsize=12, fontweight='bold')
axes[0, 0].axis('off')

axes[0, 1].imshow(image2_rgb)
axes[0, 1].set_title('Ảnh 2', fontsize=12, fontweight='bold')
axes[0, 1].axis('off')

# Vẽ histogram cho từng kênh R, G, B
for i, (color, channel_name) in enumerate(zip(colors, channel_names)):
    # Histogram ảnh 1
    hist1 = cv2.calcHist([image1], [i], None, [256], [0, 256])
    axes[i+1, 0].plot(hist1, color=color, linewidth=2)
    axes[i+1, 0].set_title(f'Ảnh 1 - Kênh {channel_name}', fontsize=11, fontweight='bold')
    axes[i+1, 0].set_xlabel('Giá trị pixel')
    axes[i+1, 0].set_ylabel('Số lượng pixel')
    axes[i+1, 0].grid(alpha=0.3)
    
    # Histogram ảnh 2
    hist2 = cv2.calcHist([image2], [i], None, [256], [0, 256])
    axes[i+1, 1].plot(hist2, color=color, linewidth=2)
    axes[i+1, 1].set_title(f'Ảnh 2 - Kênh {channel_name}', fontsize=11, fontweight='bold')
    axes[i+1, 1].set_xlabel('Giá trị pixel')
    axes[i+1, 1].set_ylabel('Số lượng pixel')
    axes[i+1, 1].grid(alpha=0.3)

plt.tight_layout()
plt.show()

# Tùy chọn: So sánh 2 histogram cùng kênh trên 1 biểu đồ
fig2, axes2 = plt.subplots(1, 3, figsize=(15, 4))

for i, (color, channel_name) in enumerate(zip(colors, channel_names)):
    hist1 = cv2.calcHist([image1], [i], None, [256], [0, 256])
    hist2 = cv2.calcHist([image2], [i], None, [256], [0, 256])
    
    axes2[i].plot(hist1, color=color, label='Ảnh 1', linewidth=2, alpha=0.7)
    axes2[i].plot(hist2, color=color, label='Ảnh 2', linewidth=2, alpha=0.7, linestyle='--')
    axes2[i].set_title(f'So sánh Kênh {channel_name}', fontsize=12, fontweight='bold')
    axes2[i].set_xlabel('Giá trị pixel')
    axes2[i].set_ylabel('Số lượng pixel')
    axes2[i].legend()
    axes2[i].grid(alpha=0.3)

plt.tight_layout()
plt.show()