import os
import sys
import subprocess
import json
from PyQt6.QtWidgets import QApplication, QWidget, QDesktopWidget, QVBoxLayout, QLabel, QPushButton, QMainWindow
from PyQt6.QtGui import QIcon, QFont, QFontDatabase

class MainWindow(QMainWindow):
        def __init__(self):
                super().__init__()
                # ------ SETUP SECTION ------
                self.setWindowTitle("System Resource Monitor")
                self.setGeometry(100,100,1000,700) # initial size (position overridden later)
                layout = QVBoxLayout() # layout - arrange widgets vertically
                self.setWindowIcon(QIcon('frontend/assets/A1BD.png')) # set icon, only works in windows
                self.center_window() # centre the window
                theme = True # TBD -- for theme-ing
                self.setTheme() # set theme according to above var^

                # ----- CONTENT --------------
                self.text()

        # --- HELPER FUNCTIONS -------

        def setTheme(self, theme):
            if(theme==True): # TBD -- create theme toggle! -- boolean var
                self.setStyleSheet("QMainWindow {background-color: #7E468A;}") # Dark theme
            else:
                self.setStyleSheet("QMainWindow {background-color: #2e3440;}") # Light theme


        def center_window(self):
                qr = self.frameGeometry()
                screen = self.screen() # for pyqt6
                cp = screen.availableGeometry().center()
                qr.moveCenter(cp)
                self.move(qr.topLeft())

        def text(self):
                label = QLabel("Hello, World!", self)
                label.setStyleSheet("font-size: 16px;")
                label.setWordWrap(True)
                label.show()

def main():
        app = QApplication(sys.argv) # app obj

        # font specification
        import os

        BASE_DIR = os.path.dirname(os.path.abspath(__file__))
        FONT_PATH = os.path.join(BASE_DIR, "assets/Roboto_Mono/static/RobotoMono-Regular.ttf")

        font_id = QFontDatabase.addApplicationFont(FONT_PATH)
        if font_id == -1:
                print("Error loading font:", FONT_PATH) #TBD! -- remove after loading all required fonts
        
        # set font
        font_families = QFontDatabase.applicationFontFamilies(font_id)
        if font_families:
                app.setFont(QFont(font_families[0], 10)) # set app font to first family, size 10
                print("Success!")
        else:
                print("No font families found for font ID:", font_id)

        window = MainWindow() # mainwindow obj called "window"
        window.show() # hidden by default. show it.
        sys.exit(app.exec()) # exit app when window closed, exec_() is built-in execute method

if __name__ == "__main__": # if running this file directly, call main
        main()