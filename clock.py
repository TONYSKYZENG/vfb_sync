import sys
from PyQt5.QtWidgets import QApplication, QWidget, QVBoxLayout, QLabel
from PyQt5.QtCore import QTimer, QTime, Qt
from PyQt5.QtGui import QFont

class ClockWindow(QWidget):
    def __init__(self):
        super().__init__()

        # 设置窗体大小为 320x480
        self.setFixedSize(320, 480)
        
        # 隐藏边框（在 framebuffer 模式下通常不需要，但可以防止某些环境下的异常）
        self.setWindowFlags(Qt.FramelessWindowHint)
        
        # 设置背景为黑色，文字为绿色（类似复古终端感）
        self.setStyleSheet("background-color: black;")

        # 布局管理
        layout = QVBoxLayout()
        
        # 时间显示标签
        self.time_label = QLabel()
        self.time_label.setAlignment(Qt.AlignCenter)
        self.time_label.setStyleSheet("color: #00FF00;")
        
        # 根据 320x480 调整字体大小
        self.time_label.setFont(QFont("Monospace", 40, QFont.Bold))
        
        layout.addWidget(self.time_label)
        self.setLayout(layout)

        # 定时器：每秒更新一次时间
        timer = QTimer(self)
        timer.timeout.connect(self.update_time)
        timer.start(1000)

        # 初始化显示
        self.update_time()

    def update_time(self):
        current_time = QTime.currentTime().toString("HH:mm:ss")
        self.time_label.setText(current_time)

if __name__ == "__main__":
    app = QApplication(sys.argv)
    
    # 强制隐藏鼠标指针（在嵌入式触摸屏上很常用）
    # app.setOverrideCursor(Qt.BlankCursor)
    
    clock = ClockWindow()
    clock.show()
    
    sys.exit(app.exec_())