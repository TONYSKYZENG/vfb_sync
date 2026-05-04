import sys
from PyQt5.QtWidgets import QApplication, QWidget, QVBoxLayout, QPushButton, QLabel
from PyQt5.QtCore import Qt

class TouchApp(QWidget):
    def __init__(self):
        super().__init__()
        self.count = 0
        self.initUI()

    def initUI(self):
        # 根据 MHS-3.5inch 屏幕参数设置固定分辨率
        self.setFixedSize(320, 480)
        self.setWindowTitle('Touch Counter')
        
        # 使用垂直布局
        layout = QVBoxLayout()
        layout.setAlignment(Qt.AlignCenter)

        # 创建按钮
        self.btn = QPushButton('add+1', self)
        #self.btn.setFixedSize(150, 60) # 增大尺寸方便电阻屏触摸
        self.btn.clicked.connect(self.increment)
        
        # 创建标签
        self.label = QLabel('0', self)
        self.label.setAlignment(Qt.AlignCenter)
        # 增大字体方便观察
        #self.label.setStyleSheet("font-size: 40px; font-weight: bold; margin-top: 20px;")

        # 将组件添加到布局
        layout.addWidget(self.btn)
        layout.addWidget(self.label)
        
        self.setLayout(layout)
        
        # 设置背景色（可选，方便调试）
        #self.setStyleSheet("background-color: #f0f0f0;")

    def increment(self):
        self.count += 1
        self.label.setText(str(self.count))
        #print('btn down')

if __name__ == '__main__':
    # 强制开启触摸转鼠标合成
    #QApplication.setAttribute(Qt.AA_SynthesizeMouseForUnhandledTouchEvents, True)
    #QApplication.setAttribute(Qt.AA_SynthesizeTouchForUnhandledMouseEvents, True)
    app = QApplication(sys.argv)
    app.setStyleSheet("""
    QWidget { background-color: black; } 
    QLabel { color: green; }
    QPushButton { 
        background-color: white; 
        color: green; 
        border-radius: 5px; 
        font-weight: bold;
    }
    QPushButton:pressed { 
        background-color: #dddddd; 
        color: darkgreen; 
    }
""")
    # 如果你在树莓派终端运行（没有桌面环境），可能需要取消下面的注释
    # import os
    # os.environ["QT_QPA_PLATFORM"] = "eglfs" 
    
    ex = TouchApp()
    ex.show()
    sys.exit(app.exec_())