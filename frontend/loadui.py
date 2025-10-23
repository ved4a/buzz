import os
from PyQt6 import uic
from PyQt6.QtWidgets import (
    QApplication,
    QMainWindow,
    QWidget,
    QVBoxLayout,
    QFrame,
    QVBoxLayout,
    QHBoxLayout,
    QLabel,
    QSlider,
    QTabWidget,
    QTableWidget,
    QCommandLinkButton,
    QHeaderView,
    QCheckBox,
    QHBoxLayout,
    QGroupBox,
    QMenu,
    QToolButton
)
from PyQt6.QtWidgets import QTabBar
from PyQt6.QtGui import QIcon, QPainter, QMouseEvent
from PyQt6.QtCore import Qt, QPoint, QRect
from gauge import GaugeWidget
import sys

# graphing imports
import pyqtgraph as pg
from pyqtgraph.Qt import QtCore, QtWidgets
import numpy as py
from PyQt6 import QtGui


class CloseableHeaderView(QHeaderView):
    def __init__(self, orientation, parent=None):
        super().__init__(orientation, parent)
        self.setSectionsClickable(True)
        self.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu) 
        self.customContextMenuRequested.connect(self.show_context_menu)
        self.sectionHidden = set()

    # allows for right click and hide menu on table headers
    def show_context_menu(self, pos): 
        logicalIndex = self.logicalIndexAt(pos)
        if logicalIndex == -1:
            return
        menu = QMenu(self)
        hide_action = menu.addAction(f"Hide column '{self.model().headerData(logicalIndex, Qt.Orientation.Horizontal)}'")
        action = menu.exec(self.mapToGlobal(pos))
        if action == hide_action:
            self.parentWidget().setColumnHidden(logicalIndex, True)
            self.sectionHidden.add(logicalIndex)
            self.update()

# UI control for toggling column visibility with closeable tabs and dropdown
class ColumnVisibilityControl(QToolButton):
    def __init__(self, table: QTableWidget, columns: list, parent=None):
        super().__init__(parent)
        self.table = table
        self.columns = columns
        self.setText("+")
        self.setPopupMode(QToolButton.ToolButtonPopupMode.InstantPopup)
        self.menu = QMenu()
        self.setMenu(self.menu)
        self.hidden_columns = []
        self.menu.aboutToShow.connect(self.update_menu)

    def update_menu(self):
        self.menu.clear()
        hidden = [i for i in range(self.table.columnCount()) if self.table.isColumnHidden(i)]
        if not hidden:
            self.menu.addAction("(no hidden columns)").setEnabled(False)
        else:
            for idx in hidden:
                col_name = self.columns[idx]
                action = self.menu.addAction(col_name)
                action.triggered.connect(lambda checked=False, i=idx: self.show_column(i))

    def show_column(self, idx):
        self.table.setColumnHidden(idx, False)
        # Remove from hidden_columns if tracked
        if idx in self.hidden_columns:
            self.hidden_columns.remove(idx)


class UI(QMainWindow):
    def __init__(self):        
        super(UI, self).__init__()
        ui_path = os.path.join(os.path.dirname(__file__), "srm.ui")
        # load ui file
        uic.loadUi(ui_path, self)

        #########################################################################
        # Prevents from having to use "self." to refer to all GUI elements
        #########################################################################

        # Header
        centralwidget = self.findChild(QWidget, "centralwidget")
        headerFrame = self.findChild(QFrame,"headerFrame")
        leftSpaceFrame = self.findChild(QFrame, "LeftSpaceFrame")
        refreshRateFrame = self.findChild(QFrame, "refreshRateFrame")
        energyGraphFrame = self.findChild(QFrame, "energyGraphFrame")
        SRMHeader = self.findChild(QFrame, "SRMHeader")
        rightSpaceFrame = self.findChild(QFrame, "RightSpaceFrame")
        refreshRateLabel = self.findChild(QLabel, "refreshRateLabel")
        horizontalSlider = self.findChild(QSlider, "horizontalSlider")

        # BIG 5 Dashboard
        self.CPUFrame = self.findChild(QFrame, "CPUFrame")
        self.GPUFrame = self.findChild(QFrame, "GPUFrame")
        self.MemoryFrame = self.findChild(QFrame, "MemoryFrame")
        self.NetworkFrame = self.findChild(QFrame, "NetworkFrame")
        self.DiskFrame = self.findChild(QFrame, "DiskFrame")

        ####################################
        # Graphs!
        ####################################

        # dashboard graphs #

        # BATTERY / ENERGY LINE GRAPH #

        # stylesheet for the frame (=> graph)
        energyGraphFrame.setStyleSheet("background: transparent; border: 0.5px solid white; border-radius: 18px;")

        # ensure the frame has a layout
        self.get_or_create_layout(energyGraphFrame, QVBoxLayout)
        
        # data
        data = [1, 3, 2, 5, 4, 6, 7, 8, 7, 6] # BACKEND!

        # plot widget
        self.energyGraph = self.create_line_graph(self.energyGraphFrame, False, data, color='#CC53FF', background=None, width=2)

        headerFixedHeight = 120 # variable
        headerFrame.setMinimumHeight(headerFixedHeight)

        # CPU GAUGE #
        cpu_gauge = self.add_gauge(self.CPUFrame, percent=49, color="#FF2D71")
        cpu_layout = self.get_or_create_layout(self.CPUFrame, QVBoxLayout)
        cpu_gauge.setSizePolicy(cpu_gauge.sizePolicy().Policy.Expanding, cpu_gauge.sizePolicy().Policy.Expanding)

        # GPU GAUGE #
        gpu_gauge = self.add_gauge(self.GPUFrame, percent=19, color="#CC53FF") 
        gpu_layout = self.get_or_create_layout(self.GPUFrame, QVBoxLayout)
        gpu_gauge.setSizePolicy(gpu_gauge.sizePolicy().Policy.Expanding, gpu_gauge.sizePolicy().Policy.Expanding)

        # MEMORY GAUGE #
        memory_gauge = self.add_gauge(self.MemoryFrame, percent=75, color="#FF9F0D")
        memory_layout = self.get_or_create_layout(self.MemoryFrame, QVBoxLayout)
        memory_gauge.setSizePolicy(memory_gauge.sizePolicy().Policy.Expanding, memory_gauge.sizePolicy().Policy.Expanding)

        # NETWORK SENT/RCVD #

        # DISK I/O READ/WRITE #

        disk_layout = self.get_or_create_layout(self.DiskFrame, QVBoxLayout)
        disk_layout.setSpacing(0)
        disk_layout.setContentsMargins(0, 0, 0, 0)
        self.DiskFrame.setStyleSheet("padding: 16px;")

        self.disk_read_label_title = QLabel("disk read rate")
        self.disk_read_label_title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.disk_read_label_title.setStyleSheet("color: white; font-size: {fontSize}px; font-family: 'Roboto Mono', monospace; border: none; background: none; border-radius: 0px; font-weight: normal; padding: 0px; margin: 0px;")
        disk_layout.addWidget(self.disk_read_label_title)

        self.disk_read_label_value = QLabel("1000 MBPS")  # BACKEND
        self.disk_read_label_value.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.disk_read_label_value.setStyleSheet("color: #7EE7F5; font-size: 25px; font-family: 'Roboto Mono', monospace; border: none; font-weight: normal; padding: 0px; margin: 0px;")
        disk_layout.addWidget(self.disk_read_label_value)

        self.disk_write_label_title = QLabel("disk write rate")
        self.disk_write_label_title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.disk_write_label_title.setStyleSheet("color: white; font-size: {fontSize}px; font-family: 'Roboto Mono', monospace; border: none; font-weight: normal; padding: 0px; margin: 0px;")
        disk_layout.addWidget(self.disk_write_label_title)

        self.disk_write_label_value = QLabel("3000 MBPS")  # BACKEND
        self.disk_write_label_value.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.disk_write_label_value.setStyleSheet("color: #7EE7F5; font-size: 25px; font-family: 'Roboto Mono', monospace; border: none; font-weight: normal; padding: 0px; margin: 0px;")
        disk_layout.addWidget(self.disk_write_label_value)

        ######################################
        # DASHBOARD - Process overview table
        #####################################

        table_frame = self.findChild(QFrame, "tableFrame")
        if table_frame:
            self.get_or_create_layout(table_frame, QVBoxLayout)

            # Set stretch factor for tableFrame in its parent layout
            parent_layout = table_frame.parentWidget().layout() if table_frame.parentWidget() else None
            if parent_layout:
                idx = parent_layout.indexOf(table_frame)
                if idx != -1:
                    parent_layout.setStretch(idx, 1)
            self.createTable(
                parent=table_frame,
                columns=["PID", "Name", "CPU %", "Memory %"],
                data=[[123, "python", 10.5, 20.1], [456, "chrome", 5.2, 15.3]],  # BACKEND
                table_name="processOverviewTable"
            )

        ####################################
        # Process Overview Tab
        ####################################

        # delete first frame (updating old ui)
        process_overview_tab = self.findChild(QWidget, "ProcessOverviewTab")
        if process_overview_tab:
            layout = process_overview_tab.layout()

            if layout and layout.count() > 0:
                first_item = layout.itemAt(0)
                first_widget = first_item.widget()

                if first_widget:
                    layout.removeWidget(first_widget)
                    first_widget.deleteLater()

            # add tableWidget to the second frame and call createTable
            if layout and layout.count() > 0:

                second_item = layout.itemAt(0)
                second_frame = second_item.widget()

                if second_frame:
                    self.createTable(
                        parent=second_frame,
                        columns=["PID", "Name", "CPU %", "Memory %"],
                        data=[[123, "python", 10.5, 20.1], [456, "chrome", 5.2, 15.3]],
                        table_name="processOverviewTable2"
                    )

    
        

        #####################################
        # Housekeeping
        #####################################


        # LAYOUTS AND SPACING
        #####################################

        # overall rules
        # Header spacing & layout

        # spacing within rightSpaceFrame
        rightSpaceFrame.setMinimumHeight(headerFixedHeight)
        leftSpaceFrame.setMinimumHeight(headerFixedHeight)
        SRMHeader.setMinimumHeight(headerFixedHeight)
        energyGraphFrame.setFixedHeight(headerFixedHeight//2) # max graph y
        refreshRateFrame.setFixedHeight(headerFixedHeight//2)

        # layouts for header frames
        refreshRateFrameLayout = self.get_or_create_layout(refreshRateFrame, QVBoxLayout)
        SRMHeaderLayout = self.get_or_create_layout(SRMHeader, QVBoxLayout)
        leftSpaceFrameLayout = self.get_or_create_layout(leftSpaceFrame, QVBoxLayout)
        rightSpaceFrameLayout = self.get_or_create_layout(rightSpaceFrame, QVBoxLayout)
        rightSpaceFrameLayout.setSpacing(0)

        # centring the heading
        # make leftSpaceFrame, SRMHeader, and rightSpaceFrame equally sized horizontally
        headerLayout = self.findChild(QtWidgets.QHBoxLayout, "horizontalLayout_3")
        if headerLayout is not None:
            headerLayout.setStretch(headerLayout.indexOf(leftSpaceFrame), 1)
            headerLayout.setStretch(headerLayout.indexOf(SRMHeader), 1)
            headerLayout.setStretch(headerLayout.indexOf(rightSpaceFrame), 1)
            leftSpaceFrame.setMinimumWidth(0)
            SRMHeader.setMinimumWidth(0)
            rightSpaceFrame.setMinimumWidth(0)
            leftSpaceFrame.setMaximumWidth(16777215)
            SRMHeader.setMaximumWidth(16777215)
            rightSpaceFrame.setMaximumWidth(16777215)

        # adding widgets if not alr present -- via method call
        self.add_widget_if_absent(rightSpaceFrameLayout, refreshRateFrame)
        self.add_widget_if_absent(rightSpaceFrameLayout, energyGraphFrame)
        self.add_widget_if_absent(refreshRateFrameLayout, refreshRateLabel)
        self.add_widget_if_absent(refreshRateFrameLayout, horizontalSlider)

        # fix layout for refreshRateFrame (fixing overlap behaviour)
        refreshRateFrameLayout = self.get_or_create_layout(refreshRateFrame, QVBoxLayout)

        # setting stretch factors for label and slider
        refreshRateFrameLayout.setStretch(refreshRateFrameLayout.indexOf(refreshRateLabel), 1)
        refreshRateFrameLayout.setStretch(refreshRateFrameLayout.indexOf(horizontalSlider), 1)

        # removing minimum height
        refreshRateFrame.setMinimumHeight(0)

        # remove top padding from centralwidget
        if centralwidget.layout() is not None:
            centralwidget.layout().setContentsMargins(
                centralwidget.layout().contentsMargins().left(),
                0,  # top
                centralwidget.layout().contentsMargins().right(),
                centralwidget.layout().contentsMargins().bottom()
            )

        # Big 5 grid frames spacing
        big5_layout = self.findChild(QtWidgets.QHBoxLayout, "horizontalLayout_2")
        if big5_layout is not None:
            big5_layout.setSpacing(15)  # can vary spacing here

        # MODIFYING/REMOVING/ADDING ELEMENTS
        #########################################
        # Remove QCommandLinkButtons and add QLabel in each frame
        frame_info = [
            (self.CPUFrame, "verticalLayout_2", "commandLinkButton", "CPU"),
            (self.GPUFrame, "verticalLayout_3", "commandLinkButton_2", "GPU"),
            (self.MemoryFrame, "verticalLayout_4", "commandLinkButton_3", "MEMORY"),
            (self.NetworkFrame, "verticalLayout_6", "commandLinkButton_5", "NETWORK"),
            (self.DiskFrame, "verticalLayout_5", "commandLinkButton_4", "DISK I/O"),
        ]
        self.big5_labels = []
        for frame, layout_name, btn_name, label_text in frame_info:
            layout = frame.findChild(QVBoxLayout, layout_name)
            btn = frame.findChild(QCommandLinkButton, btn_name)
            if btn is not None and layout is not None:
                layout.removeWidget(btn)
                btn.deleteLater()
            label = QLabel(label_text)
            label.setAlignment(Qt.AlignmentFlag.AlignCenter)
            layout.addWidget(label, alignment=Qt.AlignmentFlag.AlignHCenter)
            self.big5_labels.append(label)
            # Set stretch factors: label=0, gauge=1
            for i in range(layout.count()):
                widget = layout.itemAt(i).widget()
                if isinstance(widget, QLabel):
                    layout.setStretch(i, 0)
                elif isinstance(widget, GaugeWidget):
                    layout.setStretch(i, 1)

        # STYLING ELEMENTS
        #########################################
        CPUFrameWidth = self.CPUFrame.width()
        fontSize = CPUFrameWidth / 8
        border_radius = int(fontSize * 0.7)
        border_thickness = max(2, int(fontSize * 0.18))
        big5_stylesheet = f'''
        background-color: rgb(28, 34, 65);
        border: 2px solid rgb(255, 255, 255);
        border-radius: 15px;
        color: rgb(255,255,255);
        font-family: "Roboto Mono", monospace;
        font-size: {fontSize}px;
        font-weight: 600;
        padding: 10px 10px;
        text-align: center;
        outline: none;
        '''
        label_stylesheet = f"""
        border: {border_thickness}px solid white;
        border-radius: {border_radius}px;
        padding: {int(fontSize * 0.3)}px {int(fontSize * 0.7)}px;
        """
        for label in getattr(self, 'big5_labels', []):
            label.setStyleSheet(label_stylesheet)
            label.setSizePolicy(label.sizePolicy().Policy.Fixed, label.sizePolicy().Policy.Fixed)
            label.setFont(QtGui.QFont("Roboto Mono", int(fontSize), QtGui.QFont.Weight.Bold))
        for frame in [self.CPUFrame, self.GPUFrame, self.MemoryFrame, self.NetworkFrame, self.DiskFrame]:
            if frame is not None:
                frame.setStyleSheet(big5_stylesheet)
        
        # show the app
        self.show()
    
    def add_gauge(self, parent_frame, percent, color, background=None):
        gauge = GaugeWidget(percent, parent=parent_frame)
        if color:
            fg_color = QtGui.QColor(color)
            gauge.gauge_fg_color = fg_color
            # find a duller bg color from the (passed parameter) foreground color
            fg_hsv = fg_color.toHsv()
            bg_hue = fg_hsv.hue()
            bg_sat = max(0, int(fg_hsv.saturation() * 0.65))  # reduce saturation to 65%
            bg_val = max(0, int(fg_hsv.value() * 0.40))       # reduce brightness to 85%
            gauge.gauge_bg_color = QtGui.QColor.fromHsv(bg_hue, bg_sat, bg_val)
        if background:
            gauge.bg_color = QtGui.QColor(background)
        layout = self.get_or_create_layout(parent_frame, QVBoxLayout)
        self.add_widget_if_absent(layout, gauge)
        layout.setAlignment(Qt.AlignmentFlag.AlignCenter)
        return gauge # returns widget
    
    # parent_frame (to embed the graph), boolean to show/hide axes, data(list), colour of line, bg colour (none if transparent), line width
    def create_line_graph(self, parent_frame, showAxes, data, color='#CC53FF', background=None, width=2):
        # set layout for parent frame (if not done alr)
        layout = self.get_or_create_layout(parent_frame, QVBoxLayout)

        plot_widget = pg.PlotWidget()

        # styling the widget
        plot_widget.setStyleSheet("background: transparent; border: none;")
        plot_widget.setBackground(background)

        if(showAxes==False):
            for axis in ['bottom', 'left']:
                plot_widget.showAxis(axis, True)
                ax = plot_widget.getAxis(axis)
                ax.setStyle(showValues=False)
        
        # add this widget to the layout
        layout.addWidget(plot_widget)
        plot_widget.plot(data, pen=pg.mkPen(color=color, width=width))
        
        return plot_widget

    # add widget to layour if not alr present
    def add_widget_if_absent(self, layout, widget):
        if layout.indexOf(widget) == -1:
            layout.addWidget(widget)

    # set layout for widget (if not alr set)
    def get_or_create_layout(self, widget, layout_type=QVBoxLayout):
        if widget.layout() is None:
            layout = layout_type()
            widget.setLayout(layout)
        else:
            layout = widget.layout()
        return layout

    # parent frame, column names list, data 2d list, optional table name
    def createTable(self, parent, columns, data, table_name=None):
        # remove old table if exists
        old_table = parent.findChild(QTableWidget, table_name) if table_name else None
        if old_table:
            parent.layout().removeWidget(old_table)
            old_table.deleteLater()
        # Remove old controls
        for child in parent.findChildren(ColumnVisibilityControl):
            parent.layout().removeWidget(child)
            child.deleteLater()
        table = QTableWidget(parent)
        if table_name:
            table.setObjectName(table_name)
        table.setColumnCount(len(columns))
        table.setRowCount(len(data))
        table.setHorizontalHeaderLabels(columns)
        table.setSortingEnabled(False)
        table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        table.setSizePolicy(table.sizePolicy().Policy.Expanding, table.sizePolicy().Policy.Expanding)
        # Use custom header view
        header = CloseableHeaderView(Qt.Orientation.Horizontal, table)
        table.setHorizontalHeader(header)
        table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        layout = self.get_or_create_layout(parent, QVBoxLayout)
        # Add dropdown above table
        col_control = ColumnVisibilityControl(table, columns, parent)
        layout.insertWidget(0, col_control)
        layout.addWidget(table, stretch=1)
        # Ensure parent (tableFrame) expands inside its parent layout (dashboard vertical layout)
        parent.setSizePolicy(parent.sizePolicy().Policy.Expanding, parent.sizePolicy().Policy.Expanding)
        p = parent.parentWidget()
        if p and p.layout():
            p_layout = p.layout()
            idx = p_layout.indexOf(parent)
            if idx != -1:
                p_layout.setStretch(idx, 1)
            # if there's a Big5Frame sibling, make sure it doesn't steal space
            big5 = p.findChild(QFrame, "Big5Frame")
            if big5:
                big5_idx = p_layout.indexOf(big5)
                if big5_idx != -1:
                    p_layout.setStretch(big5_idx, 0)

        table.setMinimumWidth(600)
        for col in range(len(columns)):
            table.setColumnWidth(col, 150)
        for row_idx, row_data in enumerate(data):
            # Pad or trim row_data to match columns
            row_data_fixed = list(row_data)[:len(columns)]
            if len(row_data_fixed) < len(columns):
                row_data_fixed += ["--"] * (len(columns) - len(row_data_fixed))
            for col_idx in range(len(columns)):
                value = row_data_fixed[col_idx]
                item = QtWidgets.QTableWidgetItem(str(value))
                item.setForeground(QtGui.QColor("white"))
                table.setItem(row_idx, col_idx, item)
        table.viewport().update()  # Force table to refresh
        table.setSortingEnabled(True)
        # styling
        table.setStyleSheet(
            "QTableWidget::item { border: 1px solid white; }"
            "QHeaderView::section { background-color: #1c2241; color: white; border-top: 1px solid white; border-bottom: 1px solid white; border-left: 1px solid white; border-right: 1px solid white; }"
            "QHeaderView { color: white; }"
        )
        # Attach menu update method for restoring columns
        def update_hidden_columns_menu(idx):
            col_control.hidden_columns.append(idx)
        table.parent().update_hidden_columns_menu = update_hidden_columns_menu
        return table

app = QApplication(sys.argv)
UIWindow = UI()
app.exec()
