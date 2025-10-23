'''import sys
import json
import time
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QGridLayout, QLabel, QSlider, QTableWidget, QTableWidgetItem,
    QProgressBar, QSizePolicy
)
from PyQt5.QtCore import QTimer, QProcess, Qt, QSize
from PyQt5.QtGui import QFont, QColor, QIcon, QFontDatabase, QPainter
from PyQt5.QtChart import (
    QChart, QChartView, QPieSeries, QBarSet, QPercentBarSeries, QBarCategoryAxis
)
import pyqtgraph as pg

# --- Constants ---
BACKEND_EXECUTABLE = "./resource_monitor_backend"  # Change this to your compiled executable path!
INITIAL_REFRESH_RATE_MS = 1000  # 1 second

class SystemMonitorApp(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("System Resource Monitor")
        self.setGeometry(100, 100, 1000, 700)
        self.setStyleSheet("QMainWindow {background-color: #2e3440;}") # Dark theme

        self.cpu_usage = 0.0
        self.memory_usage = 0.0
        self.gpu_usage_history = []
        self.max_history_points = 60 # Keep 60 points (1 minute at 1s refresh)

        # QProcess for running the C++ backend
        self.process = QProcess(self)
        self.process.readyReadStandardOutput.connect(self.read_backend_output)
        self.process.errorOccurred.connect(self.handle_process_error)

        # QTimer for periodic refresh
        self.timer = QTimer(self)
        self.timer.timeout.connect(self.fetch_data)
        self.timer.start(INITIAL_REFRESH_RATE_MS)

        self.central_widget = QWidget()
        self.setCentralWidget(self.central_widget)
        self.main_layout = QVBoxLayout(self.central_widget)

        self.setup_ui()

    # ------------------- UI Setup -------------------

    def setup_ui(self):
        # Top Section: Charts and Progress Bar
        top_layout = QHBoxLayout()

        # 1. CPU Pie Chart
        self.cpu_chart_view = self.create_pie_chart("CPU Usage (%)", QColor("#8fbcbb"))
        top_layout.addWidget(self.cpu_chart_view)

        # 2. GPU Pie Chart (Placeholder data, as your backend doesn't provide it)
        self.gpu_chart_view = self.create_pie_chart("GPU Usage (%)", QColor("#b48ead"))
        top_layout.addWidget(self.gpu_chart_view)

        # 3. Memory Bar
        memory_widget = QWidget()
        mem_layout = QVBoxLayout(memory_widget)
        mem_label = QLabel("Memory Usage")
        mem_label.setStyleSheet("color: white; font-weight: bold;")
        mem_layout.addWidget(mem_label)
        self.memory_bar = self.create_memory_bar()
        mem_layout.addWidget(self.memory_bar)
        top_layout.addWidget(memory_widget)
        top_layout.setStretch(0, 1)
        top_layout.setStretch(1, 1)
        top_layout.setStretch(2, 1)

        self.main_layout.addLayout(top_layout)
        self.main_layout.addSpacing(20)
        
        # --- Middle and Bottom Sections ---
        
        # Middle Section: GPU Line Graph
        self.gpu_line_graph = self.create_line_graph()
        self.main_layout.addWidget(self.gpu_line_graph)
        self.main_layout.addSpacing(20)

        # Bottom Section: Table and Slider
        bottom_layout = QHBoxLayout()

        # 1. System Data Table
        self.table = self.create_data_table()
        bottom_layout.addWidget(self.table)
        bottom_layout.setStretch(0, 2) # Table takes more space

        # 2. Refresh Rate Slider
        slider_widget = self.create_refresh_slider()
        bottom_layout.addWidget(slider_widget)
        bottom_layout.setStretch(1, 1) # Slider takes less space

        self.main_layout.addLayout(bottom_layout)

    # ------------------- Widget Constructors -------------------

    def create_pie_chart(self, title, color):
        series = QPieSeries()
        series.append(title, 50.0) # Initial dummy value
        series.append("Free", 50.0)
        series.setHoleSize(0.35)

        # Set colors and labels
        series.slices()[0].setBrush(color)
        series.slices()[0].setLabelColor(QColor("white"))
        series.slices()[0].setLabelVisible(True)
        series.slices()[0].setLabelFont(QFont("Arial", 10, QFont.Bold))
        series.slices()[1].setBrush(QColor("#4c566a")) # Free space color
        series.slices()[1].setLabelVisible(False)
        series.setLabelsVisible(False)
        
        chart = QChart()
        chart.addSeries(series)
        chart.setTitle(title)
        chart.setBackgroundBrush(QColor("transparent"))
        chart.legend().hide()
        chart.setTitleBrush(QColor("white"))
        chart.setTitleFont(QFont("Arial", 12, QFont.Bold))

        chart_view = QChartView(chart)
        chart_view.setRenderHint(QPainter.Antialiasing)
        chart_view.setMaximumSize(300, 300)
        chart_view.setMinimumSize(200, 200)
        chart_view.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Preferred)
        chart_view.chart_series = series # Store reference to update later
        return chart_view

    def create_memory_bar(self):
        bar = QProgressBar()
        bar.setMaximum(100)
        bar.setValue(0)
        bar.setTextVisible(True)
        bar.setFormat("Memory: %p%")
        bar.setStyleSheet(
            """
            QProgressBar {
                border: 2px solid #5e81ac;
                border-radius: 5px;
                text-align: center;
                color: white;
                background-color: #4c566a;
            }
            QProgressBar::chunk {
                background-color: #a3be8c;
            }
            """
        )
        return bar

    def create_line_graph(self):
        # pyqtgraph setup
        graph = pg.PlotWidget()
        graph.setBackground('#2e3440') # Dark background
        graph.setTitle("GPU Usage Over Time", color='white', size='12pt')
        
        # Axis configuration
        styles = {'color': 'white', 'font-size': '10pt'}
        graph.setLabel('left', 'Usage (%)', **styles)
        graph.setLabel('bottom', 'Time (s)', **styles)
        graph.showGrid(x=True, y=True, alpha=0.3)
        graph.getPlotItem().setLimits(yMin=0, yMax=100)

        # Data line
        self.gpu_curve = graph.plot(
            [], [], 
            pen=pg.mkPen(color='#88c0d0', width=2), 
            name="GPU Usage"
        )
        # Placeholder for real-time history
        self.gpu_usage_history = [0] * self.max_history_points 
        
        return graph

    def create_data_table(self):
        table = QTableWidget()
        table.setRowCount(4)
        table.setColumnCount(2)
        table.setHorizontalHeaderLabels(["Metric", "Value"])

        # Set column widths
        table.setColumnWidth(0, 150)
        table.setColumnWidth(1, 150)

        # Initialize rows
        metrics = ["CPU Usage", "Memory Usage", "Refresh Rate (ms)", "GPU Temp (C)"]
        for i, metric in enumerate(metrics):
            item = QTableWidgetItem(metric)
            item.setFlags(item.flags() & ~Qt.ItemIsEditable)
            table.setItem(i, 0, item)

            value_item = QTableWidgetItem("N/A")
            value_item.setTextAlignment(Qt.AlignCenter)
            table.setItem(i, 1, value_item)

        # Style
        table.setStyleSheet(
            """
            QTableWidget {
                background-color: #3b4252;
                color: white;
                gridline-color: #4c566a;
            }
            QHeaderView::section {
                background-color: #4c566a;
                color: white;
                padding: 4px;
                border: 1px solid #2e3440;
            }
            QTableWidgetItem {
                padding: 5px;
            }
            """
        )
        table.resizeColumnsToContents()
        return table

    def create_refresh_slider(self):
        slider_widget = QWidget()
        layout = QVBoxLayout(slider_widget)
        
        # Title
        title_label = QLabel("Refresh Rate Control")
        title_label.setStyleSheet("color: white; font-weight: bold; font-size: 11pt;")
        layout.addWidget(title_label, alignment=Qt.AlignCenter)

        # Slider Value Label
        self.slider_value_label = QLabel(f"Current: {INITIAL_REFRESH_RATE_MS} ms")
        self.slider_value_label.setStyleSheet("color: #a3be8c;")
        layout.addWidget(self.slider_value_label, alignment=Qt.AlignCenter)
        
        # Slider
        self.slider = QSlider(Qt.Horizontal)
        self.slider.setRange(250, 5000) # 250ms to 5000ms
        self.slider.setValue(INITIAL_REFRESH_RATE_MS)
        self.slider.setTickInterval(500)
        self.slider.setSingleStep(50)
        self.slider.setTickPosition(QSlider.TicksBelow)
        self.slider.valueChanged.connect(self.update_refresh_rate)
        
        layout.addWidget(self.slider)
        return slider_widget

    # ------------------- Data Handling & Process Management -------------------

    def fetch_data(self):
        """Starts the backend process to get the latest data."""
        # Ensure the previous process has finished before starting a new one
        if self.process.state() == QProcess.NotRunning:
            # We don't wait for it to finish, we rely on the readyReadStandardOutput signal
            self.process.start(BACKEND_EXECUTABLE)
        else:
            # Optionally log that the process is still running
            pass 
            
    def read_backend_output(self):
        """Reads and parses the JSON output from the C++ backend."""
        output = self.process.readAllStandardOutput().data().decode().strip()
        
        try:
            # The backend prints one JSON object
            data = json.loads(output)
            
            # Update internal state
            self.cpu_usage = data.get("cpu_usage", 0.0)
            self.memory_usage = data.get("memory_usage", 0.0)
            
            # Placeholder for GPU data (not provided by your backend)
            # We'll use a dummy value for demonstration
            gpu_usage = 10.0 + 5.0 * (time.time() % 10) 
            gpu_usage = gpu_usage if gpu_usage < 100 else 99.9

            self.update_ui(gpu_usage)

        except json.JSONDecodeError as e:
            # Handle cases where the output isn't valid JSON
            print(f"JSON Decode Error: {e} - Output: {output}")
        except Exception as e:
            print(f"An unexpected error occurred: {e}")

    def handle_process_error(self, error):
        """Handles errors from the QProcess."""
        if error == QProcess.FailedToStart:
            print(f"Error: Failed to start backend executable: {BACKEND_EXECUTABLE}. Check path/permissions.")
        elif error == QProcess.Crashed:
            print(f"Error: Backend process crashed.")
        # self.update_table_item(0, 1, f"Process Error: {error.name}") # Optionally show error in UI
            
    # ------------------- UI Update Logic -------------------

    def update_ui(self, gpu_usage):
        # 1. Update Pie Charts
        self.update_pie_chart(self.cpu_chart_view.chart_series, self.cpu_usage)
        self.update_pie_chart(self.gpu_chart_view.chart_series, gpu_usage)

        # 2. Update Memory Bar
        self.memory_bar.setValue(int(self.memory_usage))

        # 3. Update Line Graph
        self.update_line_graph(gpu_usage)

        # 4. Update Table
        self.update_data_table(gpu_usage)

    def update_pie_chart(self, series: QPieSeries, usage_percent: float):
        used_slice = series.slices()[0]
        free_slice = series.slices()[1]
        
        used_slice.setValue(usage_percent)
        free_slice.setValue(100.0 - usage_percent)
        
        # Update label on the used slice
        used_slice.setLabel(f"{usage_percent:.1f}%")

    def update_line_graph(self, gpu_usage: float):
        # Update GPU history
        self.gpu_usage_history.append(gpu_usage)
        if len(self.gpu_usage_history) > self.max_history_points:
            self.gpu_usage_history.pop(0)

        # Update the plot with the new data
        self.gpu_curve.setData(self.gpu_usage_history)
        
    def update_data_table(self, gpu_usage):
        # Update values in the table
        # Row 0: CPU Usage
        self.update_table_item(0, 1, f"{self.cpu_usage:.1f}%")
        # Row 1: Memory Usage
        self.update_table_item(1, 1, f"{self.memory_usage:.1f}%")
        # Row 2: Refresh Rate
        self.update_table_item(2, 1, f"{self.timer.interval()}")
        # Row 3: GPU Temp (Placeholder)
        self.update_table_item(3, 1, f"{65 + (gpu_usage/10):.1f}°C")

    def update_table_item(self, row, col, text):
        item = self.table.item(row, col)
        if item:
            item.setText(text)
        else:
            # Create if not exists (shouldn't happen with initial setup)
            new_item = QTableWidgetItem(text)
            new_item.setTextAlignment(Qt.AlignCenter)
            self.table.setItem(row, col, new_item)

    def update_refresh_rate(self, value):
        """Updates the timer interval based on the slider value."""
        self.timer.setInterval(value)
        self.slider_value_label.setText(f"Current: {value} ms")
        # Update the table immediately
        self.update_table_item(2, 1, f"{value}")


if __name__ == '__main__':
    # Make sure to compile your C++ code first:
    # g++ -std=c++17 your_backend_file.cpp -o resource_monitor_backend -I<path_to_nlohmann_json>
    # The executable must be in the same directory as this Python script, or adjust BACKEND_EXECUTABLE.
    
    app = QApplication(sys.argv)
    window = SystemMonitorApp()
    window.show()
    sys.exit(app.exec_())'''