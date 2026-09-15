# ==============================================================================
# Программа для анализа и визуализации отклонений сырых данных IMU
# (Без внешних пакетов – только базовый R)
# ==============================================================================

# --- КОНФИГУРАЦИЯ ---
file_path <- "MPU_raw_data_.txt" # Название файла где сырые данные
# Константы для перевода сырых данных в физические величины
const_ac <- 16384 # Акселерометр
const_gs <- 131.0 # Гироскоп

# 2. Функция для загрузки данных из текстового файла
load_imu_data <- function(path) {
  cat(paste("Загрузка данных из файла:", path, "\n"))
  
  if (!file.exists(path)) {
    stop(paste("Ошибка: Файл не найден по пути:", path))
  }
  
  # Читаем как CSV без заголовка, удаляем лишние пробелы
  data <- read.csv(path, header = FALSE, strip.white = TRUE)
  
  # Проверяем количество столбцов
  if (ncol(data) != 5) {
    stop(paste("Ожидалось 5 столбцов, а получено", ncol(data)))
  }
  
  # Даём понятные имена столбцам (базовый R)
  colnames(data) <- c("raw_ax", "raw_ay", "raw_az", "raw_gx", "raw_gy")
  
  cat("Первые 5 строк данных:\n")
  print(head(data, 5))
  cat("Структура данных:\n")
  str(data)
  
  cat(paste("Успешно загружено", nrow(data), "строк данных.\n"))
  
  return(data)
}

# --- ИМИТАЦИЯ ДАННЫХ (если файла нет) ---
if (!file.exists(file_path)) {
  cat("Создание временного тестового файла для демонстрации...\n")
  n_rows <- 1000
  set.seed(42)
  test_data <- data.frame(
    raw_ax = rnorm(n_rows, mean = 100, sd = 10),
    raw_ay = rnorm(n_rows, mean = 50, sd = 5),
    raw_az = rnorm(n_rows, mean = 10, sd = 2),
    raw_gx = rnorm(n_rows, mean = 0.1, sd = 0.05),
    raw_gy = rnorm(n_rows, mean = -0.05, sd = 0.01)
  )
  write.table(test_data, file_path, row.names = FALSE,
              col.names = FALSE, sep = ", ")
}
# ----------------------------------------------------------------------


# 3. Основная функция для анализа и визуализации (базовая графика)
analyze_imu_data <- function(data) {
  
  cat("\n--- Шаг 1: Расчет средних значений ---\n")
  
  # Вычисляем средние для каждого канала
  mean_ax <- mean(data$raw_ax/const_ac)
  mean_ay <- mean(data$raw_ay/const_ac)
  mean_az <- mean(data$raw_az/const_ac)
  mean_gx <- mean(data$raw_gx/const_gs)
  mean_gy <- mean(data$raw_gy/const_gs)
  
  cat("Средние значения:\n")
  cat(sprintf("AX: %.2f, AY: %.2f, AZ: %.2f, GX: %.4f, GY: %.4f\n", 
              mean_ax, mean_ay, mean_az, mean_gx, mean_gy))
  
  # Отклонения
  dev_ax <- data$raw_ax/const_ac - mean_ax
  dev_ay <- data$raw_ay/const_ac - mean_ay
  dev_az <- data$raw_az/const_ac - mean_az
  dev_gx <- data$raw_gx/const_gs - mean_gx
  dev_gy <- data$raw_gy/const_gs - mean_gy
  
  cat("Расчет отклонений завершен.\n")
  
  # 4. Визуализация (базовая графика R)
  cat("\n--- Шаг 2: Создание графиков отклонений ---\n")
  
  # Устанавливаем параметры отображения: 2 строки, 2 столбца
  par(mfrow = c(2, 2))
  
  # 1) Отклонение акселерометра AX
  plot((dev_ax), type = "p", pch = 16, cex = 0.5, col = "blue",
       xlab = expression("Время, с ("*отсчет %*% 33 %*% 10^-3 *")"), 
       ylab = "Отклонение (g)",
       main = paste("Отклонение AX (среднее =", round(mean_ax, 2), ")"),
       xaxs = "i", yaxs = "i")
  abline(h = 0, col = "red", lwd = 1.5)
  
  # 2) Отклонение акселерометра AY
  plot((dev_ay), type = "p", pch = 16, cex = 0.5, col = "darkgreen",
       xlab = expression("Время, с ("*отсчет %*% 33 %*% 10^-3 *")"),
       ylab = "Отклонение (g)",
       main = paste("Отклонение AY (среднее =", round(mean_ay, 2), ")"),
       xaxs = "i", yaxs = "i")
  abline(h = 0, col = "red", lwd = 1.5)
  
  # 3) Отклонение гироскопа GX
  plot((dev_gx), type = "p", pch = 16, cex = 0.5, col = "purple",
       xlab = expression("Время, с ("*отсчет %*% 33 %*% 10^-3 *")"),
       ylab = "Отклонение (град/сек)",
       main = paste("Отклонение GX (среднее =", round(mean_gx, 4), ")"),
       xaxs = "i", yaxs = "i")
  abline(h = 0, col = "red", lwd = 1.5)
  
  # 4) Отклонение гироскопа GY
  plot((dev_gy), type = "p", pch = 16, cex = 0.5, col = "orange",
       xlab = expression("Время, с ("*отсчет %*% 33 %*% 10^-3 *")"),
       ylab = "Отклонение (град/сек)",
       main = paste("Отклонение GY (среднее =", round(mean_gy, 4), ")"),
       xaxs = "i", yaxs = "i")
  abline(h = 0, col = "red", lwd = 1.5)
  
  # Возвращаем стандартный вывод на один график
  par(mfrow = c(1, 1))
  cat("\nГрафики построены.\n")
}


# ==============================================================================
# 5. ЗАПУСК
# ==============================================================================

tryCatch({
  imu_data <- load_imu_data(file_path)
  analyze_imu_data(imu_data)
}, error = function(e) {
  cat("\n!!! ПРОИЗОШЛА ОШИБКА !!!\n")
  cat(e$message, "\n")
  cat("Проверьте, что файл существует и содержит 5 столбцов чисел, разделённых запятыми.\n")
  cat("Пример корректной строки: 292, -136, 18828, -84, 247\n")
})
