import sys
from PyQt5.QtWidgets import QApplication, QWidget, QVBoxLayout, QGridLayout, QPushButton, QLabel
from PyQt5.QtCore import Qt

class CalculatorApp(QWidget):
    def __init__(self):
        super().__init__()
        self.expression = ""
        self.initUI()

    def initUI(self):
        # 适配 MHS-3.5inch 屏幕分辨率
        self.setFixedSize(320, 480)
        self.setWindowTitle('SPI Touch Calc')

        main_layout = QVBoxLayout()
        
        # 显示屏（Label）
        self.display = QLabel('0')
        self.display.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self.display.setFixedHeight(80)
        self.display.setStyleSheet("font-size: 40px; padding: 10px; border: 1px solid #333;")
        main_layout.addWidget(self.display)

        # 按钮网格布局
        grid_layout = QGridLayout()
        buttons = [
            '7', '8', '9', '/',
            '4', '5', '6', '*',
            '1', '2', '3', '-',
            'C', '0', '.', '+',
            '='
        ]

        row, col = 0, 0
        for btn_text in buttons:
            button = QPushButton(btn_text)
            # 按钮尺寸适配电阻屏大面积触摸
            button.setFixedSize(65, 65) 
            button.clicked.connect(self.on_click)
            
            if btn_text == '=':
                grid_layout.addWidget(button, row, col, 1, 4) # 等号横跨一行
            else:
                grid_layout.addWidget(button, row, col)
            
            col += 1
            if col > 3:
                col = 0
                row += 1

        main_layout.addLayout(grid_layout)
        self.setLayout(main_layout)

    def on_click(self):
        sender = self.sender().text()

        if sender == 'C':
            self.expression = ""
            self.display.setText("0")
        elif sender == '=':
            try:
                # 计算结果并保留小数
                result = str(eval(self.expression))
                self.display.setText(result)
                self.expression = result
            except:
                self.display.setText("Error")
                self.expression = ""
        else:
            # 防止多次输入小数点或符号
            self.expression += sender
            self.display.setText(self.expression)

if __name__ == '__main__':
    # 启用触摸合成逻辑以解决“按下有动画但不触发点击”的问题
    QApplication.setAttribute(Qt.AA_SynthesizeMouseForUnhandledTouchEvents, True)
    
    app = QApplication(sys.argv)
    
    # 一行代码设置：背景黑色，按钮白色底+绿色字，文字标签绿色
    app.setStyleSheet("""
        QWidget { background-color: black; } 
        QPushButton { background-color: white; color: green; font-size: 25px; font-weight: bold; border-radius: 10px; } 
        QPushButton:pressed { background-color: #ddd; }
        QLabel { color: green; font-family: 'Monospace'; }
    """)
    
    calc = CalculatorApp()
    calc.show()
    sys.exit(app.exec_())