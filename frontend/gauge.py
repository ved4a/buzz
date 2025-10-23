import sys
from PyQt6.QtWidgets import QWidget, QApplication, QSizePolicy
from PyQt6.QtGui import QPainter, QPen, QFont, QColor
from PyQt6.QtCore import Qt, QRectF

class GaugeWidget(QWidget):
    def __init__(self, percent=0, parent=None):
        super().__init__(parent)
        self.percent = percent
        self.setMinimumSize(70, 70)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        self.gauge_bg_color = QColor(60, 62, 90)
        self.gauge_fg_color = QColor(170, 85, 255)
        self.text_color = Qt.GlobalColor.white

    def setPercent(self, percent):
        self.percent = max(0, min(100, percent))
        self.update()

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        rect = self.rect() # used for positioning

        # Margin and thickness are proportional to widget size
        min_dim = min(rect.width(), rect.height())
        margin = max(2, int(min_dim * 0.08))  # 8% margin
        size = min_dim - margin * 2
        center = rect.center()
        gauge_rect = QRectF(center.x() - size/2, center.y() - size/2, size, size)
        thickness = max(4, int(size * 0.13))  # 13% of gauge size

        # save the painter state before rotating
        painter.save()
        # rotate the gauge arcs 90° clockwise around the center
        painter.translate(center)
        painter.rotate(90)
        painter.translate(-center)

        # Drawing the background arc (empty bit)
        pen_bg = QPen(self.gauge_bg_color, thickness, Qt.PenStyle.SolidLine, Qt.PenCapStyle.RoundCap)
        painter.setPen(pen_bg)
        painter.drawArc(gauge_rect, 45 * 16, 270 * 16)

        # Draw foreground arc (filled bit)
        pen_fg = QPen(self.gauge_fg_color, thickness, Qt.PenStyle.SolidLine, Qt.PenCapStyle.RoundCap)
        painter.setPen(pen_fg)
        span_angle = int(270 * self.percent / 100) # draw only at that % angle
        # Start the foreground arc at the left end (315°), which is 45+270=315
        painter.drawArc(gauge_rect, (45 + 270 - span_angle) * 16, span_angle * 16)

        # restore painter state so text is not also rotated
        painter.restore()

        # Draw percentage text (not rotated)
        painter.setPen(self.text_color)
        font = QFont("Roboto Mono", max(8, int(size/3.5)))
        painter.setFont(font)
        text = f"{self.percent}%"
        painter.drawText(gauge_rect, Qt.AlignmentFlag.AlignCenter, text)

if __name__ == "__main__":
    app = QApplication(sys.argv)
    w = GaugeWidget(66)
    w.show()
    sys.exit(app.exec())