package com.lagradost.cloudstream3.linux

import java.awt.BorderLayout
import java.awt.Color
import java.awt.Dimension
import java.awt.FlowLayout
import java.awt.Font
import java.awt.GridBagConstraints
import java.awt.GridBagLayout
import java.awt.Insets
import java.awt.event.WindowAdapter
import java.awt.event.WindowEvent
import java.io.File
import java.net.URI
import java.net.http.HttpClient
import java.net.http.HttpRequest
import java.net.http.HttpResponse
import java.nio.file.Files
import java.time.Duration
import java.util.Properties
import javax.swing.BorderFactory
import javax.swing.DefaultListModel
import javax.swing.JFileChooser
import javax.swing.JButton
import javax.swing.JDialog
import javax.swing.JFrame
import javax.swing.JLabel
import javax.swing.JList
import javax.swing.JMenu
import javax.swing.JMenuBar
import javax.swing.JMenuItem
import javax.swing.JOptionPane
import javax.swing.JPanel
import javax.swing.JScrollPane
import javax.swing.JSplitPane
import javax.swing.JTextArea
import javax.swing.JTextField
import javax.swing.ListSelectionModel
import javax.swing.SwingUtilities
import javax.swing.UIManager
import javax.swing.border.EmptyBorder

private const val DEFAULT_REPOSITORY = "https://github.com/recloudstream/extensions"

private class PreferencesStore {
    private val file = File(System.getProperty("user.home"), ".config/cloudstream-linux/preferences.properties")
    private val values = Properties()

    init {
        if (file.isFile) file.inputStream().use(values::load)
    }

    fun get(key: String, fallback: String = "") = values.getProperty(key, fallback)

    fun set(key: String, value: String) {
        values.setProperty(key, value)
        file.parentFile.mkdirs()
        file.outputStream().use { values.store(it, "CloudStream Linux preferences") }
    }
}

private class LinuxCloudStream : JFrame("CloudStream Linux") {
    private val preferences = PreferencesStore()
    private val repositories = DefaultListModel<String>()
    private val repositoryList = JList(repositories)
    private val status = JLabel("Ready")
    private val http = HttpClient.newBuilder().connectTimeout(Duration.ofSeconds(10)).build()

    init {
        configureTheme()
        defaultCloseOperation = DO_NOTHING_ON_CLOSE
        minimumSize = Dimension(900, 620)
        size = Dimension(1100, 720)
        setLocationByPlatform(true)
        contentPane.background = Color(0x16, 0x17, 0x1C)
        jMenuBar = menuBar()
        loadRepositories()
        add(buildLayout())
        addWindowListener(object : WindowAdapter() {
            override fun windowClosing(e: WindowEvent) {
                dispose()
            }
        })
    }

    private fun configureTheme() {
        val surface = Color(0x20, 0x21, 0x27)
        val text = Color(0xF2, 0xF2, 0xF7)
        UIManager.put("Panel.background", Color(0x16, 0x17, 0x1C))
        UIManager.put("OptionPane.background", surface)
        UIManager.put("OptionPane.messageForeground", text)
        UIManager.put("Label.foreground", text)
        UIManager.put("Button.background", Color(0x31, 0x32, 0x3A))
        UIManager.put("Button.foreground", text)
        UIManager.put("Button.select", Color(0x5B, 0x4B, 0xA8))
        UIManager.put("TextField.background", Color(0x29, 0x2A, 0x31))
        UIManager.put("TextField.foreground", text)
        UIManager.put("List.background", Color(0x20, 0x21, 0x27))
        UIManager.put("List.foreground", text)
        UIManager.put("List.selectionBackground", Color(0x4F, 0x42, 0x88))
        UIManager.put("List.selectionForeground", Color.WHITE)
    }

    private fun menuBar() = JMenuBar().apply {
        val file = JMenu("File")
        file.add(JMenuItem("Play URL…").apply { addActionListener { showPlayDialog() } })
        file.addSeparator()
        file.add(JMenuItem("Quit").apply { addActionListener { dispose() } })
        add(file)
        val help = JMenu("Help")
        help.add(JMenuItem("About CloudStream Linux").apply {
            addActionListener { JOptionPane.showMessageDialog(this@LinuxCloudStream, "CloudStream Linux\nA native desktop frontend for the CloudStream project.", "About", JOptionPane.INFORMATION_MESSAGE) }
        })
        add(help)
    }

    private fun navigationRail(): JPanel = JPanel().apply {
        preferredSize = Dimension(190, 0)
        background = Color(0x20, 0x21, 0x27)
        layout = FlowLayout(FlowLayout.CENTER, 10, 18)
        listOf("⌂  Home", "▣  Library", "⇩  Downloads", "⚙  Settings").forEachIndexed { index, label ->
            add(JButton(label).apply {
                preferredSize = Dimension(165, 42)
                horizontalAlignment = JButton.LEFT
                isFocusPainted = false
                isContentAreaFilled = true
                background = if (index == 0) Color(0x4F, 0x42, 0x88) else Color(0x20, 0x21, 0x27)
                foreground = Color(0xF2, 0xF2, 0xF7)
                border = BorderFactory.createEmptyBorder(8, 14, 8, 8)
                addActionListener {
                    when (index) {
                        1 -> showLibraryDialog()
                        2 -> showDownloadsDialog()
                        3 -> showSettingsDialog()
                    }
                }
            })
        }
    }

    private fun buildLayout(): JPanel {
        val root = JPanel(BorderLayout(0, 0)).apply { background = Color(0x16, 0x17, 0x1C) }
        val header = JPanel(FlowLayout(FlowLayout.LEFT, 20, 16)).apply {
            background = Color(0x20, 0x21, 0x27)
            border = EmptyBorder(0, 8, 0, 8)
        }
        header.add(JLabel("CLOUDSTREAM").apply { foreground = Color(0xF0, 0xF0, 0xF3); font = Font("SansSerif", Font.BOLD, 20) })
        header.add(JLabel("Linux desktop").apply { foreground = Color(0xA8, 0xA9, 0xB2) })
        root.add(header, BorderLayout.NORTH)
        root.add(navigationRail(), BorderLayout.WEST)

        val home = JPanel(GridBagLayout()).apply { background = Color(0x16, 0x17, 0x1C); border = EmptyBorder(36, 42, 36, 42) }
        val c = GridBagConstraints().apply { fill = GridBagConstraints.HORIZONTAL; weightx = 1.0; insets = Insets(8, 0, 8, 0) }
        c.gridy = 0
        home.add(JLabel("Your media center").apply { foreground = Color.WHITE; font = Font("SansSerif", Font.BOLD, 30) }, c)
        c.gridy++
        home.add(JLabel("Add extension repositories, then play a direct media URL with your system player.").apply { foreground = Color(0xB9, 0xBA, 0xC4) }, c)
        c.gridy++
        home.add(JLabel("CloudStream does not provide video sources by default.").apply { foreground = Color(0xD8, 0xA8, 0x65) }, c)
        c.gridy++
        val play = JButton("Play a media URL").apply { addActionListener { showPlayDialog() } }
        home.add(play, c)
        c.gridy++
        home.add(JLabel("Repositories").apply { foreground = Color.WHITE; font = Font("SansSerif", Font.BOLD, 18) }, c)
        c.gridy++
        val split = JSplitPane(JSplitPane.HORIZONTAL_SPLIT, JScrollPane(repositoryList), repositoryActions())
        split.resizeWeight = 0.62
        split.preferredSize = Dimension(700, 240)
        home.add(split, c)
        root.add(JScrollPane(home), BorderLayout.CENTER)
        root.add(status.apply { foreground = Color(0xA8, 0xA9, 0xB2); border = EmptyBorder(8, 14, 8, 14) }, BorderLayout.SOUTH)
        return root
    }

    private fun repositoryActions(): JPanel = JPanel(FlowLayout(FlowLayout.LEFT, 8, 8)).apply {
        background = Color(0x20, 0x21, 0x27)
        add(JButton("Add repository").apply { addActionListener { addRepository() } })
        add(JButton("Remove").apply { addActionListener {
            repositories.removeElement(repositoryList.selectedValue)
            saveRepositories()
        } })
        add(JButton("Open selected").apply { addActionListener {
            repositoryList.selectedValue?.let { DesktopSupport.open(it) }
        } })
    }

    private fun loadRepositories() {
        val stored = preferences.get("repositories", DEFAULT_REPOSITORY)
        stored.split("\n").filter(String::isNotBlank).forEach(repositories::addElement)
    }

    private fun saveRepositories() {
        preferences.set("repositories", (0 until repositories.size).joinToString("\n") { repositories[it] })
        status.text = "Repositories saved"
    }

    private fun showLibraryDialog() {
        val model = DefaultListModel<File>()
        val list = JList(model).apply {
            selectionMode = ListSelectionModel.SINGLE_SELECTION
            cellRenderer = object : javax.swing.DefaultListCellRenderer() {
                override fun getListCellRendererComponent(list: javax.swing.JList<*>?, value: Any?, index: Int, selected: Boolean, focus: Boolean): java.awt.Component {
                    val component = super.getListCellRendererComponent(list, value, index, selected, focus)
                    text = (value as? File)?.name ?: value.toString()
                    return component
                }
            }
        }
        fun refresh() {
            model.clear()
            val roots = listOfNotNull(preferences.get("libraryFolder").takeIf(String::isNotBlank)?.let(::File), File(System.getProperty("user.home"), "Videos"))
            roots.distinct().filter(File::isDirectory).forEach { root ->
                runCatching {
                    Files.walk(root.toPath()).use { paths ->
                        paths.filter(Files::isRegularFile).map { it.toFile() }.filter { it.extension.lowercase() in setOf("mp4", "mkv", "webm", "avi", "mov", "m4v", "mp3", "flac", "ogg") }.limit(500).forEach(model::addElement)
                    }
                }
            }
        }
        val dialog = JDialog(this, "Library", true)
        val content = JPanel(BorderLayout(12, 12)).apply { border = EmptyBorder(18, 18, 18, 18); background = Color(0x16, 0x17, 0x1C) }
        content.add(JLabel("Local media").apply { font = Font("SansSerif", Font.BOLD, 22) }, BorderLayout.NORTH)
        content.add(JScrollPane(list), BorderLayout.CENTER)
        val actions = JPanel(FlowLayout(FlowLayout.LEFT)).apply {
            background = Color(0x16, 0x17, 0x1C)
            add(JButton("Choose folder").apply { addActionListener {
                val chooser = JFileChooser(preferences.get("libraryFolder", System.getProperty("user.home"))).apply { fileSelectionMode = JFileChooser.DIRECTORIES_ONLY }
                if (chooser.showOpenDialog(dialog) == JFileChooser.APPROVE_OPTION) { preferences.set("libraryFolder", chooser.selectedFile.absolutePath); refresh() }
            } })
            add(JButton("Play selected").apply { addActionListener { list.selectedValue?.let { DesktopSupport.play(it.absolutePath, preferences.get("player", "mpv")) } } })
            add(JButton("Close").apply { addActionListener { dialog.dispose() } })
        }
        content.add(actions, BorderLayout.SOUTH)
        dialog.contentPane = content
        dialog.setSize(720, 520)
        dialog.setLocationRelativeTo(this)
        refresh()
        dialog.isVisible = true
    }

    private fun showDownloadsDialog() {
        val folder = File(preferences.get("downloadFolder", File(System.getProperty("user.home"), "Downloads").absolutePath))
        val message = "Downloads are stored in:\n${folder.absolutePath}\n\nUse the system file manager to manage downloaded media."
        val result = JOptionPane.showOptionDialog(this, message, "Downloads", JOptionPane.DEFAULT_OPTION, JOptionPane.INFORMATION_MESSAGE, null, arrayOf("Open folder", "Close"), "Open folder")
        if (result == 0) { folder.mkdirs(); DesktopSupport.open(folder.toURI().toString()) }
    }

    private fun showSettingsDialog() {
        val player = JTextField(preferences.get("player", "mpv"), 28)
        val folder = JTextField(preferences.get("downloadFolder", File(System.getProperty("user.home"), "Downloads").absolutePath), 28)
        val panel = JPanel(GridBagLayout()).apply { background = Color(0x20, 0x21, 0x27); border = EmptyBorder(12, 12, 12, 12) }
        val c = GridBagConstraints().apply { insets = Insets(7, 7, 7, 7); fill = GridBagConstraints.HORIZONTAL; weightx = 1.0 }
        c.gridx = 0; c.gridy = 0; panel.add(JLabel("Player executable"), c)
        c.gridx = 1; panel.add(player, c)
        c.gridx = 0; c.gridy = 1; panel.add(JLabel("Download folder"), c)
        c.gridx = 1; panel.add(folder, c)
        if (JOptionPane.showConfirmDialog(this, panel, "Settings", JOptionPane.OK_CANCEL_OPTION, JOptionPane.PLAIN_MESSAGE) == JOptionPane.OK_OPTION) {
            preferences.set("player", player.text.trim().ifBlank { "mpv" })
            preferences.set("downloadFolder", folder.text.trim().ifBlank { File(System.getProperty("user.home"), "Downloads").absolutePath })
            status.text = "Settings saved"
        }
    }

    private fun addRepository() {
        val input = JTextField(38)
        val panel = JPanel(BorderLayout(0, 8)).apply {
            add(JLabel("HTTPS repository URL:"), BorderLayout.NORTH)
            add(input, BorderLayout.CENTER)
        }
        if (JOptionPane.showConfirmDialog(this, panel, "Add repository", JOptionPane.OK_CANCEL_OPTION) == JOptionPane.OK_OPTION) {
            val url = input.text.trim()
            if (url.startsWith("https://")) { repositories.addElement(url); saveRepositories() }
            else JOptionPane.showMessageDialog(this, "Use an HTTPS URL.", "Invalid repository", JOptionPane.WARNING_MESSAGE)
        }
    }

    private fun showPlayDialog() {
        val url = JTextField(preferences.get("lastUrl"), 44)
        val panel = JPanel(BorderLayout(0, 8)).apply {
            add(JLabel("Direct video/audio URL:"), BorderLayout.NORTH)
            add(url, BorderLayout.CENTER)
        }
        if (JOptionPane.showConfirmDialog(this, panel, "Play media", JOptionPane.OK_CANCEL_OPTION) == JOptionPane.OK_OPTION) {
            val value = url.text.trim()
            runCatching { URI(value) }.onSuccess {
                preferences.set("lastUrl", value)
                DesktopSupport.play(value, preferences.get("player", "mpv"))
                status.text = "Started playback with ${preferences.get("player", "mpv")}"
            }.onFailure { JOptionPane.showMessageDialog(this, "Enter a valid URL.", "Invalid URL", JOptionPane.WARNING_MESSAGE) }
        }
    }
}

private object DesktopSupport {
    fun open(url: String) {
        runCatching { ProcessBuilder("xdg-open", url).start() }
    }

    fun play(url: String, player: String) {
        val executable = if (player.isBlank()) "mpv" else player
        ProcessBuilder(executable, "--force-window=yes", url).start()
    }
}

fun main() {
    if (java.awt.GraphicsEnvironment.isHeadless()) {
        System.err.println("CloudStream Linux requires a graphical X11 or Wayland session.")
        return
    }
    UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName())
    SwingUtilities.invokeLater { LinuxCloudStream().isVisible = true }
}
