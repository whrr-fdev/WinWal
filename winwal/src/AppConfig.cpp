#include <QTimer>
static QTimer *s_saveDebounceTimer = nullptr;
#include "AppConfig.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QStringList>
#include <QMap>
#include <algorithm>

AppConfig::AppConfig(QObject *parent) : QObject(parent) {
    initTranslations();
    load();
}

QString AppConfig::configFilePath() const {
    return QDir::homePath() + "/.config/hypr/scripts/control_center.conf";
}

void AppConfig::initTranslations() {
    m_enDict = {
        // Боковое меню и карточки приветствия
        // Кнопки, переключатели и остальные вкладки
        {"Поиск по настройкам", "Search settings"},
        {"󰂚  Mako", "󰂚  Mako"},
        {"Уведомления, таймаут, цвета", "Notifications, timeout, colors"},
        {"Уведомления (Mako)", "Notifications (Mako)"},
        {"󰂚  Тест уведомления", "󰂚  Test Notification"},
        {"󰎟  Очистить все", "󰎟  Dismiss All"},
        {"Расположение и геометрия", "Position & Geometry"},
        {"Позиция на экране:", "Screen anchor:"},
        {"Ширина уведомления:", "Notification width:"},
        {"Макс. высота:", "Max height:"},
        {"Внешний отступ (Margin):", "Outer margin:"},
        {"Внутренний отступ (Padding):", "Inner padding:"},
        {"Оформление, рамка и цвета", "Appearance, Border & Colors"},
        {"Прозрачность фона Mako:", "Mako background opacity:"},
        {"Скругление углов:", "Corner radius:"},
        {"Цвет фона уведомлений:", "Notification background color:"},
        {"Цвет текста:", "Text color:"},
        {"Цвет обычной рамки:", "Normal border color:"},
        {"Цвет рамки важных (Critical):", "Critical urgency border color:"},
        {"󰂚  Тест обычного", "󰂚  Test Normal"},
        {"󰀨  Тест важного", "󰀨  Test Critical"},
        {"Поведение и таймаут", "Behavior & Timeout"},
        {"Время показа (мс):", "Display timeout:"},
        {"Макс. уведомлений на экране:", "Max visible notifications:"},
        {"Показывать иконки приложений:", "Show application icons:"},
        {"↺   Сбросить настройки Mako по умолчанию", "↺   Reset Mako Settings to Defaults"},
        {"Выравнивание экрана приветствия:", "Welcome screen alignment:"},
        {"Слева", "Left"},
        {"По центру", "Center"},
        {"Справа", "Right"},
        {"На всех экранах", "All Screens"},
        {"Только приветствие", "Welcome Only"},
        {"Выкл", "Off"},
        {"Вкл", "On"},
        {"В играх", "In Games"},
        {"По часовой", "Clockwise"},
        {"Против часовой", "Counter-CW"},
        {"Навстречу", "Symmetrical"},
        {"Монохром", "Monochrome"},
        {"Холодный лед", "Cold Ice"},
        {"Аврора", "Aurora"},
        {"Теплый неон", "Warm Neon"},
        {"Свой цвет", "Custom Color"},
        {"Кольцо", "Ring"},
        {"Импульс", "Pulse"},
        {"Звезда", "Star"},
        {"Двойная волна", "Double Wave"},
        {"Слайд", "Slide"},
        {"Кроссфейд", "Crossfade"},
        {"Глубина", "Depth"},
        {"Пружина", "Bounce"},
        {"Каскад", "Cascade"},
        {"Воронка", "Black Hole"},
        {"Портал", "Portal"},
        {"Телепорт", "Teleport"},
        {"Все", "All"},
        {"Видео", "Video"},
        {"Фото", "Photo"},
        {"Только файлы", "Files Only"},
        {"Только приложения", "Apps Only"},
        {"Домашняя (~)", "Home (~)"},
        {"Вся система (/)", "Full System (/)"},
        {"1м", "1m"}, {"2м", "2m"}, {"5м", "5m"}, {"10м", "10m"}, {"30м", "30m"},
        {"Интернет", "Internet"},
        {"Документы", "Documents"},
        {"Разработка", "Development"},
        {"Мультимедиа", "Multimedia"},
        {"Графика", "Graphics"},
        {"Система", "System"},
        {"Игры", "Games"},
        {"Утилиты", "Utilities"},
        {"Клавиатура", "Keyboard"},
        {"Мышь", "Mouse"},
        {"Боковые кнопки", "Side Buttons"},
        {"Колесико", "Scroll Wheel"},
        {"Отображать только комбинации клавиш (Ctrl/Alt/Super/Shift)", "Show key combinations only (Ctrl/Alt/Super/Shift)"},
        {"24-часовой формат", "24-hour format"},
        {"Секунды", "Seconds"},
        {"Дата", "Date"},
        {"День недели", "Day of week"},
        {"🕒   Часы Peaclock (Foot)", "🕒   Peaclock Clock (Foot)"},
        {"󰣇   Информер Fastfetch (Foot)", "󰣇   Fastfetch Info (Foot)"},
        {"󰍹   Монитор процессов Btop (Kitty)", "󰍹   Btop Process Monitor (Kitty)"},
        {"Обычная (Normal)", "Normal"},
        {"Отразить (Flipped)", "Flipped"},
        {"Отразить 90°", "Flipped 90°"},
        {"Отразить 180°", "Flipped 180°"},
        {"Отразить 270°", "Flipped 270°"},
        {"Новый пресет", "New Preset"},
        {"Трава", "Grass"},
        {"Сакура", "Sakura"},
        {"✓ Готово", "✓ Done"},
        {"Выбор цвета", "Color Picker"},
        {"Применить", "Apply"},
        {"Сохранить", "Save"},
        {"Удалить", "Delete"},
        {"Добавить", "Add"},
        {" + Добавить", " + Add"},
        {"+ Добавить", "+ Add"},
        {"Обновить", "Refresh"},
        {"Подключить", "Connect"},
        {"Отключить", "Disconnect"},
        {"Поиск...", "Searching..."},
        {"Выключить все эффекты (Normal)", "Disable All Effects (Normal)"},
        {"Шейдеры экрана (GLSL)", "Screen Shaders (GLSL)"},
        {"Настройки курсора и лупы", "Cursor & Magnifier Settings"},
        {"Устройства ввода", "Input Devices"},
        {"Беспроводные сети Wi-Fi", "Wi-Fi Wireless Networks"},
        {"Bluetooth устройства", "Bluetooth Devices"},
        {"Анимации и кривые Безье", "Animations & Bezier Curves"},
        {"Настройка панели Waybar", "Waybar Panel Settings"},
        {"Автозапуск приложений и служб", "Startup Applications & Services"},
        {"Горячие клавиши (Keybinds)", "Keybinds & Shortcuts"},
        {"Правила окон (Window Rules)", "Window Rules"},
        {"Поиск", "Search"},
        {"Окна", "Windows"},
        {"Анимации", "Animations"},
        {"Курсор", "Cursor"},
        {"Мониторы", "Monitors"},
        {"Устройства", "Devices"},
        {"Сеть", "Network"},
        {"Блютуз", "Bluetooth"},
        {"Автозапуск", "Autostart"},
        {"Бинды", "Keybinds"},
        {"Правила окон", "Window Rules"},
        {"Приложения", "Applications"},
                {"Атмосфера", "Atmosphere"},
        {"󰖗  Атмосфера", "󰖗  Atmosphere"},
        {"Дождь, снег, листья и физика", "Rain, snow, leaves & physics"},
        {"Живая атмосфера (Live Atmosphere)", "Live Atmosphere"},
        {"󰖝  Порыв ветра", "󰖝  Wind Gust"},
        {"󰃢  Стряхнуть капли и сугробы", "󰃢  Clear Drops & Snow"},
        {"Режим работы и сезонные профили", "Operation Mode & Seasonal Profiles"},
        {"Внутри окна HySett (Песочница с физикой UI):", "Inside HySett window (UI physics sandbox):"},
        {"Глобальный оверлей рабочего стола (Waybar и окна Hyprland):", "Global desktop overlay (Waybar & Hyprland windows):"},
        {"🌧  Ливень", "🌧  Downpour"},
        {"🍂  Осень (Дождь + Листья)", "🍂  Autumn (Rain + Leaves)"},
        {"❄  Зима (Снег и сугробы)", "❄  Winter (Snow & Drifts)"},
        {"🍁  Листопад", "🍁  Falling Leaves"},
        {"Погодные слои: Дождь, Снег и Листья", "Weather Layers: Rain, Snow & Leaves"},
        {"Включить дождь:", "Enable rain:"},
        {"Интенсивность дождя:", "Rain intensity:"},
        {"Эффект «мокрого стекла» и стекающие капли:", "Wet glass effect & trickling drops:"},
        {"Включить снегопад:", "Enable snowfall:"},
        {"Интенсивность снега:", "Snow intensity:"},
        {"Накопление сугробов на кнопках, карточках и Waybar:", "Accumulate snowdrifts on buttons, cards & Waybar:"},
        {"Добавить падающие листья:", "Add falling leaves:"},
        {"Количество листьев:", "Leaves amount:"},
        {"Палитра листьев:", "Leaves palette:"},
        {"Багряная осень", "Crimson Autumn"},
        {"Золотой клен", "Golden Maple"},
        {"Лес", "Forest"},
        {"Физика, ветер и интерактивность с окнами", "Physics, Wind & Window Interactivity"},
        {"Сила и направление ветра:", "Wind strength & direction:"},
        {"Турбулентность воздуха:", "Air turbulence:"},
        {"Гравитация (скорость падения):", "Gravity (fall speed):"},
        {"Размер частиц:", "Particle scale:"},
        {"Коллизии со всеми элементами внутри HySett (карточки, кнопки, слайдеры):", "Collide with all UI elements inside HySett:"},
        {"Интерактивность с панелью Waybar (препятствие для капель и сугробов):", "Interact with Waybar panel (obstacle for drops & snow):"},
        {"Интерактивность с окнами Hyprland (сброс воды/снега вниз при закрытии окна):", "Interact with Hyprland windows (drop water/snow on close):"},
        {"Конденсат на открытых окнах спустя пару секунд:", "Condensation on open windows after a few seconds:"},
        {"Реакция частиц и сдувание листьев курсором мыши:", "Particle reaction & leaf blowing by mouse cursor:"},
        {"Оптимизация и частота кадров (FPS)", "Optimization & Frame Rate (FPS)"},
        {"Лимит FPS погоды:", "Weather FPS limit:"},
        {"Автопауза (0% GPU) при полноэкранных играх и видео:", "Auto-pause (0% GPU) on fullscreen games & video:"},
        {"↺   Сбросить настройки атмосферы по умолчанию", "↺   Reset Atmosphere Settings to Defaults"},
        {"Шейдеры", "Shaders"},
        {"Обои", "Wallpapers"},
        {"Виджеты", "Widgets"},
        {"Настройки", "Settings"},
        {"Закрыть", "Close"},
        {"Привет, Whrr!", "Hello, Whrr!"},
        {"󰍉  Поиск", "󰍉  Search"},
        {"Файлы системы и программы", "System files & applications"},
        {"󰖲  Окна", "󰖲  Windows"},
        {"Рамки, блюр, прозрачность", "Borders, blur, opacity"},
        {"󱐌  Анимации", "󱐌  Animations"},
        {"Скорости и кривые Безье", "Speeds & Bezier curves"},
        {"󰆿  Курсор", "󰆿  Cursor"},
        {"Тема, размер, лупа", "Theme, size, magnifier"},
        {"󰍹  Мониторы", "󰍹  Monitors"},
        {"Разрешение, Гц, VRR", "Resolution, Hz, VRR"},
        {"󰌌  Устройства", "󰌌  Devices"},
        {"Клавиатура, мышь, тачпад", "Keyboard, mouse, touchpad"},
        {"󰖩  Сеть", "󰖩  Network"},
        {"Wi-Fi сети и подключения", "Wi-Fi networks & connections"},
        {"󰂯  Блютуз", "󰂯  Bluetooth"},
        {"Беспроводные устройства", "Wireless devices"},
        {"󱓞  Waybar", "󱓞  Waybar"},
        {"Позиция, отступы, пилюли", "Position, margins, pills"},
        {"󰒲  Автозапуск", "󰒲  Autostart"},
        {"Службы при старте системы", "Startup services & apps"},
        {"⌨  Бинды", "⌨  Keybinds"},
        {"Горячие клавиши Hyprland", "Hyprland hotkeys"},
        {"󰘳  Правила окон", "󰘳  Window Rules"},
        {"Плавающие окна и размеры", "Floating windows & sizes"},
        {"󰀻  Приложения", "󰀻  Applications"},
        {"Лаунчер системных программ", "System app launcher"},
        {"󰏘  Шейдеры", "󰏘  Shaders"},
        {"Эффекты экрана и CRT", "Screen effects & CRT"},
        {"󰸉  Обои", "󰸉  Wallpapers"},
        {"Галерея, пресеты и демон", "Gallery, presets & daemon"},
        {"󰕰  Виджеты", "󰕰  Widgets"},
        {"Часы, оверлей клавиш, сводка", "Clock, key overlay, stats"},
        {"󰒓  Настройки", "󰒓  Settings"},
        {"Шрифт, масштаб, стекло", "Font, scale, liquid glass"},
        // Поиск и Настройки
        {"Глобальный поиск (Файлы и Приложения)", "Global Search (Files & Applications)"},
        {"󰑐  Поиск...", "󰑐  Searching..."},
        {"Введите название файла, папки или приложения...", "Type file, folder, or application name..."},
        {"Показать:", "Show:"},
        {"Область:", "Scope:"},
        {"󰉋 Папка", "󰉋 Folder"},
        {"󰆏 Путь", "󰆏 Copy Path"},
        {"Папка", "Folder"},
        {"Настройки центра управления", "Control Center Settings"},
        {"Интерфейс", "Interface"},
        {"Язык интерфейса (Language):", "Interface Language:"},
        {"Запоминать последнюю открытую вкладку:", "Remember last opened tab:"},
        {"Высота окна (при след. запуске):", "Window height (on next launch):"},
        {"Масштаб интерфейса (UI Scale):", "Interface scale (UI Scale):"},
        {"Размер шрифта:", "Font size:"},
        {"Текст бокового меню:", "Sidebar text alignment:"},
        {"Отображаемые пункты в боковом меню", "Visible Sidebar Menu Items"},
        {"Показать все", "Show All"},
        {"Цвета и палитра оформления", "Colors & Theme Palette"},
        {"Синхронизировать цвета с основным акцентом:", "Sync colors with main accent:"},
        {"Основной акцентный цвет:", "Main accent color:"},
        {"Цвет свечения границ:", "Border glow color:"},
        {"Цвет курсора и вспышки:", "Cursor & click flash color:"},
        {"Цвет фона окна:", "Window background color:"},
        {"Цвет карточек и панелей:", "Cards & panels color:"},
        {"Сбросить цвета", "Reset Colors"},
        {"Изменить", "Change"},
        {"Подсветка границ окна и курсор", "Window Border Glow & Cursor"},
        {"Режим отображения границ:", "Border display mode:"},
        {"Направление движения:", "Movement direction:"},
        {"Палитра свечения:", "Glow palette:"},
        {"Яркость свечения границ:", "Border glow brightness:"},
        {"Ширина свечения у края:", "Border glow spread:"},
        {"Скорость движения света:", "Light movement speed:"},
        {"Сила свечения и следа от курсора:", "Cursor glow & wake strength:"},
        {"Форма вспышки / волны при клике:", "Click flash / wave shape:"},
        {"Сила вспышки при клике:", "Click flash strength:"},
        {"Анимации переходов между вкладками", "Tab Transition Animations"},
        {"Переход с экрана приветствия:", "Welcome screen transition:"},
        {"Тип анимации между вкладками:", "Tab switch animation:"},
        {"Длительность переходов:", "Transition duration:"},
        {"Прозрачность и эффекты стекла (Liquid Glass)", "Transparency & Liquid Glass"},
        {"Жидкое стекло для кнопок, тумблеров и ползунков:", "Liquid Glass for buttons, switches & sliders:"},
        {"Главный фон (Окно):", "Main background (Window):"},
        {"Интенсивность блюра окна:", "Window blur intensity:"},
        {"Прозрачность диалоговых окон:", "Dialog windows opacity:"},
        {"Блюр диалоговых окон:", "Dialog windows blur:"},
        {"Искажение стекла (Refraction):", "Glass refraction:"},
        {"Боковое меню:", "Sidebar opacity:"},
        {"Карточки и панели:", "Cards & panels opacity:"},
        {"↺   Сбросить настройки центра по умолчанию", "↺   Reset Control Center Settings to Defaults"},
        {"Выкл", "Off"},
        // Обои, Окна, Мониторы, Виджеты, Приложения
        {"Обои рабочего стола", "Desktop Wallpapers"},
        {"Фоновый демон и режим", "Background Daemon & Mode"},
        {"Автосмена обоев:", "Auto-change wallpapers:"},
        {"Режим:", "Mode:"},
        {"Интервал:", "Interval:"},
        {"Случайная анимация:", "Random animation:"},
        {"Скругления окон:", "Rounded corners:"},
        {"Прозрачность вкладки:", "Tab opacity:"},
        {"Блюр вкладки:", "Tab blur:"},
        {"󰒝   Случайные обои", "󰒝   Random Wallpaper"},
        {"Пресеты", "Presets"},
        {"+ Создать", "+ Create"},
        {"Создание нового пресета", "Create New Preset"},
        {"Название:", "Name:"},
        {"Путь к обоям:", "Wallpaper path:"},
        {"📁 Обзор...", "📁 Browse..."},
        {"✏  Записать текущие Обои + Окна", "✏  Capture Current Wallpaper + Windows"},
        {"Прозрачность окон:", "Active window opacity:"},
        {"Неактивные окна:", "Inactive windows:"},
        {"Блюр окон:", "Window blur:"},
        {"Размер блюра:", "Blur size:"},
        {"Сохранить пресет", "Save Preset"},
        {"Окна и оформление рабочего стола", "Windows & Desktop Appearance"},
        {"Прозрачность активного окна:", "Active window opacity:"},
        {"Прозрачность неактивного:", "Inactive window opacity:"},
        {"Затемнять неактивные:", "Dim inactive windows:"},
        {"Сила затемнения:", "Dim strength:"},
        {"Затемнение вокруг (dimaround):", "Dim around:"},
        {"Блюр:", "Blur:"},
        {"Проходы блюра:", "Blur passes:"},
        {"Насыщенность блюра:", "Blur vibrancy:"},
        {"Тени:", "Shadows:"},
        {"Размер теней:", "Shadow range:"},
        {"Отступы между окнами:", "Gaps in:"},
        {"Внешние отступы:", "Gaps out:"},
        {"Толщина рамки:", "Border size:"},
        {"Анимации:", "Animations:"},
        {"Цвет активной рамки (RGBA):", "Active border color (RGBA):"},
        {"Цвет неактивной рамки (RGBA):", "Inactive border color (RGBA):"},
        {"Сбросить к hyprland.lua", "Reset to hyprland.lua"},
        {"Дисплей и Мониторы", "Display & Monitors"},
        {"↺  Обновить список", "↺  Refresh List"},
        {"Разрешение и частота:", "Resolution & refresh rate:"},
        {"Масштабирование (Scale):", "Scaling (Scale):"},
        {"Ориентация экрана:", "Screen orientation:"},
        {"Настройки дисплея (Display Settings)", "Display Settings"},
        {"Адаптивная частота (VRR / FreeSync):", "Variable Refresh Rate (VRR / FreeSync):"},
        {"Пробуждать экран мышью (DPMS):", "Wake screen on mouse move (DPMS):"},
        {"Пробуждать экран клавишами (DPMS):", "Wake screen on key press (DPMS):"},
        {"Сохранить новое разрешение?", "Keep new display settings?"},
        {"✓  Сохранить", "✓  Keep Changes"},
        {"Вернуть назад", "Revert"},
        {"Приложения системы", "System Applications"},
        {"Поиск приложений...", "Search applications..."},
        {"Виджеты и оверлеи", "Widgets & Overlays"},
        {"Оверлей клавиш", "Key Overlay"},
        {"Часы на фоне", "Desktop Clock"},
        {"Информеры", "System Informers"},
        {"Оверлей клавиатуры и мыши", "Keyboard & Mouse Overlay"},
        {"Компоненты отображения", "Display Components"},
        {"Положение и управление", "Position & Controls"},
        {"Расположение на экране:", "Screen position:"},
        {"󰈈  Скрыть/Показать (Super + F9)", "󰈈  Show/Hide (Super + F9)"},
        {"↺  Перезапустить процесс", "↺  Restart Process"},
        {"Часы на рабочем столе", "Desktop Clock"},
        {"Размер часов:", "Clock size:"},
        {"Прозрачность часов:", "Clock opacity:"},
        {"󰆓  Сохранить параметры", "󰆓  Save Settings"},
        {"↺  Перезапустить часы", "↺  Restart Clock"},
        {"Мониторы процессов и системная сводка", "Process Monitors & System Info"}
    };
}

QString AppConfig::trAuto(const QString &ruText) const {
    if (m_language != "en") return ruText;
    auto it = m_enDict.constFind(ruText);
    if (it != m_enDict.constEnd()) return it.value();
    if (ruText.startsWith("↩ Откатить")) {
        QString s = ruText;
        return s.replace("↩ Откатить", "↩ Undo");
    }
    if (ruText.startsWith("Файлы и папки")) {
        QString s = ruText;
        return s.replace("Файлы и папки", "Files & Folders");
    }
    if (ruText.startsWith("Настройка пресета")) {
        QString s = ruText;
        return s.replace("Настройка пресета", "Edit Preset");
    }
    if (ruText.startsWith("Откат к прежнему режиму через")) {
        QString s = ruText;
        s.replace("Откат к прежнему режиму через", "Reverting in");
        s.replace("сек...", "sec...");
        return s;
    }
    return ruText;
}

void AppConfig::syncHyprlandLuaHeight(int h) const {
    QString luaPath = QDir::homePath() + "/.config/hypr/hyprland.lua";
    QFile f(luaPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QString content = QString::fromUtf8(f.readAll());
    f.close();

    QRegularExpression re1(R"((\["com\.whrr\.HyprSettings"\]\s*=\s*\{\s*x\s*=\s*1040\s*,\s*y\s*=\s*)\d+(\s*\}))");
    content.replace(re1, QString("\\1%1\\2").arg(h));

    QRegularExpression re2(R"((match\s*=\s*\{\s*class\s*=\s*"com\.whrr\.HyprSettings"\s*\}[\s\S]{1,120}?size\s*=\s*\{\s*1040\s*,\s*)\d+(\s*\}))");
    content.replace(re2, QString("\\1%1\\2").arg(h));

    if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        f.write(content.toUtf8());
        f.close();
    }
}

void AppConfig::load() {
    QFile file(configFilePath());
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty() || line.startsWith('#')) continue;
            int eq = line.indexOf('=');
            if (eq == -1) continue;
            QString key = line.left(eq).trimmed();
            QString val = line.mid(eq + 1).trimmed();

            if (key == "font_size") m_fontSize = std::clamp(val.toInt(), 9, 14);
            else if (key == "ui_scale") m_uiScale = std::clamp(val.toDouble(), 0.8, 1.3);
            else if (key == "window_height") m_windowHeight = std::clamp(val.toInt(), 560, 960);
            else if (key == "align_center") m_alignCenter = (val == "1" || val == "true");
            else if (key == "welcome_align_mode") m_welcomeAlignMode = std::clamp(val.toInt(), 0, 2);
            else if (key == "sidebar_width") m_sidebarWidth = std::clamp(val.toInt(), 110, 260);
            else if (key == "opacity") m_opacity = std::clamp(val.toDouble(), 0.0, 1.0);
            else if (key == "sidebar_opacity") m_sidebarOpacity = std::clamp(val.toDouble(), 0.0, 1.0);
            else if (key == "card_opacity") m_cardOpacity = std::clamp(val.toDouble(), 0.0, 1.0);
            else if (key == "wallpapers_opacity") m_wallpapersOpacity = std::clamp(val.toDouble(), 0.0, 1.0);
            else if (key == "wallpapers_blur") m_wallpapersBlur = std::clamp(val.toDouble(), 0.0, 1.0);
            else if (key == "dialog_opacity") m_dialogOpacity = std::clamp(val.toDouble(), 0.05, 1.0);
            else if (key == "dialog_blur") m_dialogBlur = std::clamp(val.toDouble(), 0.0, 1.0);
            else if (key == "auto_glass") m_autoGlass = (val == "1" || val == "true");
            else if (key == "anim_type") m_animType = val;
            else if (key == "welcome_anim_type") m_welcomeAnimType = val;
            else if (key == "anim_duration") m_animDuration = std::clamp(val.toInt(), 0, 1000);
            else if (key == "remember_last_tab") m_rememberLastTab = (val == "1" || val == "true");
            else if (key == "hidden_sidebar_tabs") m_hiddenSidebarTabs = val;
            else if (key == "language") m_language = (val == "en" ? "en" : "ru");
            else if (key == "border_glow_enabled") m_borderGlowEnabled = (val == "1" || val == "true");
            else if (key == "border_glow_all_pages") m_borderGlowAllPages = (val == "1" || val == "true");
            else if (key == "border_glow_intensity") m_borderGlowIntensity = std::clamp(val.toDouble(), 0.0, 1.0);
            else if (key == "border_glow_spread") m_borderGlowSpread = std::clamp(val.toDouble(), 0.1, 1.0);
            else if (key == "border_glow_speed") m_borderGlowSpeed = std::clamp(val.toDouble(), 0.1, 2.5);
            else if (key == "border_glow_color_mode") m_borderGlowColorMode = std::clamp(val.toInt(), 0, 4);
            else if (key == "border_glow_direction") m_borderGlowDirection = std::clamp(val.toInt(), 0, 2);
            else if (key == "cursor_glow_intensity") m_cursorGlowIntensity = std::clamp(val.toDouble(), 0.0, 1.0);
            else if (key == "click_flash_intensity") m_clickFlashIntensity = std::clamp(val.toDouble(), 0.0, 1.0);
            else if (key == "click_flash_shape") m_clickFlashShape = std::clamp(val.toInt(), 0, 3);
            else if (key == "sync_colors_with_accent") m_syncColorsWithAccent = (val == "1" || val == "true");
            else if (key == "accent_color" && QColor::isValidColorName(val)) m_accentColor = QColor(val);
            else if (key == "border_glow_custom_color" && QColor::isValidColorName(val)) m_borderGlowCustomColor = QColor(val);
            else if (key == "cursor_custom_color" && QColor::isValidColorName(val)) m_cursorCustomColor = QColor(val);
            else if (key == "window_bg_color" && QColor::isValidColorName(val)) m_windowBgColor = QColor(val);
            else if (key == "card_bg_color" && QColor::isValidColorName(val)) m_cardBgColor = QColor(val);
        }
    }
}

void AppConfig::save() {
    QMap<QString, QString> extra;
    

    extra["font_size"] = QString::number(m_fontSize);
    extra["ui_scale"] = QString::number(m_uiScale, 'f', 2);
    extra["window_height"] = QString::number(m_windowHeight);
    extra["align_center"] = m_alignCenter ? "1" : "0";
    extra["welcome_align_mode"] = QString::number(m_welcomeAlignMode);
    extra["sidebar_width"] = QString::number(m_sidebarWidth);
    extra["opacity"] = QString::number(m_opacity, 'f', 2);
    extra["sidebar_opacity"] = QString::number(m_sidebarOpacity, 'f', 2);
    extra["card_opacity"] = QString::number(m_cardOpacity, 'f', 2);
    extra["wallpapers_opacity"] = QString::number(m_wallpapersOpacity, 'f', 2);
    extra["wallpapers_blur"] = QString::number(m_wallpapersBlur, 'f', 2);
    extra["dialog_opacity"] = QString::number(m_dialogOpacity, 'f', 2);
    extra["dialog_blur"] = QString::number(m_dialogBlur, 'f', 2);
    extra["auto_glass"] = m_autoGlass ? "1" : "0";
    extra["anim_type"] = m_animType;
    extra["welcome_anim_type"] = m_welcomeAnimType;
    extra["anim_duration"] = QString::number(m_animDuration);
    extra["remember_last_tab"] = m_rememberLastTab ? "1" : "0";
    extra["hidden_sidebar_tabs"] = m_hiddenSidebarTabs;
    extra["language"] = m_language;
    extra["border_glow_enabled"] = m_borderGlowEnabled ? "1" : "0";
    extra["border_glow_all_pages"] = m_borderGlowAllPages ? "1" : "0";
    extra["border_glow_intensity"] = QString::number(m_borderGlowIntensity, 'f', 2);
    extra["border_glow_spread"] = QString::number(m_borderGlowSpread, 'f', 2);
    extra["border_glow_speed"] = QString::number(m_borderGlowSpeed, 'f', 2);
    extra["border_glow_color_mode"] = QString::number(m_borderGlowColorMode);
    extra["border_glow_direction"] = QString::number(m_borderGlowDirection);
    extra["cursor_glow_intensity"] = QString::number(m_cursorGlowIntensity, 'f', 2);
    extra["click_flash_intensity"] = QString::number(m_clickFlashIntensity, 'f', 2);
    extra["click_flash_shape"] = QString::number(m_clickFlashShape);
    extra["sync_colors_with_accent"] = m_syncColorsWithAccent ? "1" : "0";
    extra["accent_color"] = m_accentColor.name(QColor::HexRgb);
    extra["border_glow_custom_color"] = m_borderGlowCustomColor.name(QColor::HexRgb);
    extra["cursor_custom_color"] = m_cursorCustomColor.name(QColor::HexRgb);
    extra["window_bg_color"] = m_windowBgColor.name(QColor::HexRgb);
    extra["card_bg_color"] = m_cardBgColor.name(QColor::HexRgb);

    QFile file(configFilePath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        for (auto it = extra.begin(); it != extra.end(); ++it) {
            out << it.key() << "=" << it.value() << "\n";
        }
    }
}

QColor AppConfig::accentTextColor() const {
    double lum = 0.299 * m_accentColor.redF() + 0.587 * m_accentColor.greenF() + 0.114 * m_accentColor.blueF();
    return lum > 0.52 ? QColor("#000000") : QColor("#ffffff");
}

bool AppConfig::isTabVisible(int idx) const {
    if (m_hiddenSidebarTabs.trimmed().isEmpty()) return true;
    const QStringList parts = m_hiddenSidebarTabs.split(',', Qt::SkipEmptyParts);
    return !parts.contains(QString::number(idx));
}

void AppConfig::setTabVisible(int idx, bool visible) {
    QStringList parts = m_hiddenSidebarTabs.split(',', Qt::SkipEmptyParts);
    QString sIdx = QString::number(idx);
    if (visible) parts.removeAll(sIdx);
    else if (!parts.contains(sIdx)) parts.append(sIdx);
    setHiddenSidebarTabs(parts.join(','));
}

void AppConfig::setHiddenSidebarTabs(const QString &v) {
    if (m_hiddenSidebarTabs != v) { m_hiddenSidebarTabs = v; emit hiddenSidebarTabsChanged(); save(); }
}

void AppConfig::setLanguage(const QString &v) {
    QString clean = (v == "en" ? "en" : "ru");
    if (m_language != clean) { m_language = clean; emit languageChanged(); save(); }
}

void AppConfig::setFontSize(int v) { v = std::clamp(v, 9, 14); if (m_fontSize != v) { m_fontSize = v; emit fontSizeChanged(); save(); } }
void AppConfig::setUiScale(qreal v) { v = std::clamp(v, 0.8, 1.3); if (!qFuzzyCompare(m_uiScale, v)) { m_uiScale = v; emit uiScaleChanged(); save(); } }
void AppConfig::setWindowHeight(int v) { v = std::clamp(v, 560, 960); if (m_windowHeight != v) { m_windowHeight = v; emit windowHeightChanged(); save(); syncHyprlandLuaHeight(v); } }
void AppConfig::setAlignCenter(bool v) { if (m_alignCenter != v) { m_alignCenter = v; emit alignCenterChanged(); save(); } }
void AppConfig::setWelcomeAlignMode(int v) { v = std::clamp(v, 0, 2); if (m_welcomeAlignMode != v) { m_welcomeAlignMode = v; emit welcomeAlignModeChanged(); save(); } }
void AppConfig::setSidebarWidth(int v) { v = std::clamp(v, 110, 260); if (m_sidebarWidth != v) { m_sidebarWidth = v; emit sidebarWidthChanged(); save(); } }
void AppConfig::setOpacity(qreal v) { v = std::clamp(v, 0.0, 1.0); if (!qFuzzyCompare(m_opacity, v)) { m_opacity = v; emit opacityChanged(); save(); } }
void AppConfig::setSidebarOpacity(qreal v) { v = std::clamp(v, 0.0, 1.0); if (!qFuzzyCompare(m_sidebarOpacity, v)) { m_sidebarOpacity = v; emit sidebarOpacityChanged(); save(); } }
void AppConfig::setCardOpacity(qreal v) { v = std::clamp(v, 0.0, 1.0); if (!qFuzzyCompare(m_cardOpacity, v)) { m_cardOpacity = v; emit cardOpacityChanged(); save(); } }
void AppConfig::setWallpapersOpacity(qreal v) { v = std::clamp(v, 0.0, 1.0); if (!qFuzzyCompare(m_wallpapersOpacity, v)) { m_wallpapersOpacity = v; emit wallpapersOpacityChanged(); save(); } }
void AppConfig::setWallpapersBlur(qreal v) { v = std::clamp(v, 0.0, 1.0); if (!qFuzzyCompare(m_wallpapersBlur, v)) { m_wallpapersBlur = v; emit wallpapersBlurChanged(); save(); } }
void AppConfig::setDialogOpacity(qreal v) { v = std::clamp(v, 0.05, 1.0); if (!qFuzzyCompare(m_dialogOpacity, v)) { m_dialogOpacity = v; emit dialogOpacityChanged(); save(); } }
void AppConfig::setDialogBlur(qreal v) { v = std::clamp(v, 0.0, 1.0); if (!qFuzzyCompare(m_dialogBlur, v)) { m_dialogBlur = v; emit dialogBlurChanged(); save(); } }
void AppConfig::setAutoGlass(bool v) { if (m_autoGlass != v) { m_autoGlass = v; emit autoGlassChanged(); save(); } }
void AppConfig::setAnimType(const QString &v) { if (m_animType != v) { m_animType = v; emit animTypeChanged(); save(); } }
void AppConfig::setWelcomeAnimType(const QString &v) { if (m_welcomeAnimType != v) { m_welcomeAnimType = v; emit welcomeAnimTypeChanged(); save(); } }
void AppConfig::setAnimDuration(int v) { v = std::clamp(v, 0, 1000); if (m_animDuration != v) { m_animDuration = v; emit animDurationChanged(); save(); } }
void AppConfig::setRememberLastTab(bool v) { if (m_rememberLastTab != v) { m_rememberLastTab = v; emit rememberLastTabChanged(); save(); } }
void AppConfig::setBorderGlowEnabled(bool v) { if (m_borderGlowEnabled != v) { m_borderGlowEnabled = v; emit borderGlowEnabledChanged(); save(); } }
void AppConfig::setBorderGlowAllPages(bool v) { if (m_borderGlowAllPages != v) { m_borderGlowAllPages = v; emit borderGlowAllPagesChanged(); save(); } }
void AppConfig::setBorderGlowIntensity(qreal v) { v = std::clamp(v, 0.0, 1.0); if (!qFuzzyCompare(m_borderGlowIntensity, v)) { m_borderGlowIntensity = v; emit borderGlowIntensityChanged(); save(); } }
void AppConfig::setBorderGlowSpread(qreal v) { v = std::clamp(v, 0.1, 1.0); if (!qFuzzyCompare(m_borderGlowSpread, v)) { m_borderGlowSpread = v; emit borderGlowSpreadChanged(); save(); } }
void AppConfig::setBorderGlowSpeed(qreal v) { v = std::clamp(v, 0.1, 2.5); if (!qFuzzyCompare(m_borderGlowSpeed, v)) { m_borderGlowSpeed = v; emit borderGlowSpeedChanged(); save(); } }
void AppConfig::setBorderGlowColorMode(int v) { v = std::clamp(v, 0, 4); if (m_borderGlowColorMode != v) { m_borderGlowColorMode = v; emit borderGlowColorModeChanged(); save(); } }
void AppConfig::setBorderGlowDirection(int v) { v = std::clamp(v, 0, 2); if (m_borderGlowDirection != v) { m_borderGlowDirection = v; emit borderGlowDirectionChanged(); save(); } }
void AppConfig::setCursorGlowIntensity(qreal v) { v = std::clamp(v, 0.0, 1.0); if (!qFuzzyCompare(m_cursorGlowIntensity, v)) { m_cursorGlowIntensity = v; emit cursorGlowIntensityChanged(); save(); } }
void AppConfig::setClickFlashIntensity(qreal v) { v = std::clamp(v, 0.0, 1.0); if (!qFuzzyCompare(m_clickFlashIntensity, v)) { m_clickFlashIntensity = v; emit clickFlashIntensityChanged(); save(); } }
void AppConfig::setClickFlashShape(int v) { v = std::clamp(v, 0, 3); if (m_clickFlashShape != v) { m_clickFlashShape = v; emit clickFlashShapeChanged(); save(); } }

void AppConfig::setSyncColorsWithAccent(bool v) {
    if (m_syncColorsWithAccent != v) {
        m_syncColorsWithAccent = v;
        if (v) {
            m_borderGlowCustomColor = m_accentColor;
            m_cursorCustomColor = m_accentColor;
            m_borderGlowColorMode = 4;
            emit borderGlowCustomColorChanged();
            emit cursorCustomColorChanged();
            emit borderGlowColorModeChanged();
        }
        emit syncColorsWithAccentChanged();
        save();
    }
}

void AppConfig::setAccentColor(const QColor &c) {
    if (!c.isValid() || m_accentColor == c) return;
    m_accentColor = c;
    emit accentColorChanged();
    if (m_syncColorsWithAccent) {
        m_borderGlowCustomColor = c;
        m_cursorCustomColor = c;
        m_borderGlowColorMode = 4;
        emit borderGlowCustomColorChanged();
        emit cursorCustomColorChanged();
        emit borderGlowColorModeChanged();
    }
    save();
}

void AppConfig::setBorderGlowCustomColor(const QColor &c) {
    if (!c.isValid() || m_borderGlowCustomColor == c) return;
    m_borderGlowCustomColor = c;
    m_borderGlowColorMode = 4;
    emit borderGlowCustomColorChanged();
    emit borderGlowColorModeChanged();
    save();
}

void AppConfig::setCursorCustomColor(const QColor &c) {
    if (!c.isValid() || m_cursorCustomColor == c) return;
    m_cursorCustomColor = c;
    emit cursorCustomColorChanged();
    save();
}

void AppConfig::setWindowBgColor(const QColor &c) {
    if (!c.isValid() || m_windowBgColor == c) return;
    m_windowBgColor = c;
    emit windowBgColorChanged();
    save();
}

void AppConfig::setCardBgColor(const QColor &c) {
    if (!c.isValid() || m_cardBgColor == c) return;
    m_cardBgColor = c;
    emit cardBgColorChanged();
    save();
}

void AppConfig::resetColors() {
    m_syncColorsWithAccent = true;
    m_accentColor = QColor("#ffffff");
    m_borderGlowCustomColor = QColor("#ffffff");
    m_cursorCustomColor = QColor("#ffffff");
    m_windowBgColor = QColor("#0a0a0c");
    m_cardBgColor = QColor("#141419");
    m_borderGlowColorMode = 0;
    emit syncColorsWithAccentChanged();
    emit accentColorChanged();
    emit borderGlowCustomColorChanged();
    emit cursorCustomColorChanged();
    emit windowBgColorChanged();
    emit cardBgColorChanged();
    emit borderGlowColorModeChanged();
    save();
}

void AppConfig::resetDefaults() {
    setFontSize(11);
    setUiScale(1.0);
    setWindowHeight(720);
    setAlignCenter(false);
    setWelcomeAlignMode(1);
    setSidebarWidth(136);
    setOpacity(0.70);
    setSidebarOpacity(0.00);
    setCardOpacity(0.10);
    setWallpapersOpacity(0.59);
    setWallpapersBlur(0.15);
    setDialogOpacity(0.45);
    setDialogBlur(0.65);
    setAutoGlass(true);
    setAnimType("blackhole");
    setWelcomeAnimType("blackhole");
    setAnimDuration(150);
    setRememberLastTab(false);
    setHiddenSidebarTabs("");
    setBorderGlowEnabled(true);
    setBorderGlowAllPages(true);
    setBorderGlowIntensity(0.75);
    setBorderGlowSpread(0.45);
    setBorderGlowSpeed(1.00);
    setBorderGlowDirection(0);
    setCursorGlowIntensity(0.65);
    setClickFlashIntensity(0.70);
    setClickFlashShape(0);
    resetColors();
}

QString AppConfig::consumeLaunchCmd() {
    QFile f(QDir::tempPath() + "/winwal_launch_cmd");
    if (!f.exists()) return "";
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return "";
    QString cmd = QString::fromUtf8(f.readAll()).trimmed();
    f.close();
    f.remove();
    return cmd;
}

bool AppConfig::isDaemonStart() const {
    return qEnvironmentVariable("HYSETT_DAEMON_START") == "1";
}
