#include <iostream>
#include <string>
#include <filesystem>
#include <map>
#include <argparse/argparse.hpp>

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QUrl>
#include <QSslError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

// Use Qt private API for ZIP extraction
#include <QtCore/private/qzipreader_p.h>

namespace fs = std::filesystem;

// Template configuration
struct TemplateConfig {
    std::string name;
    std::string githubRepo;
    std::string defaultVersion;
};

// Available templates
const std::map<std::string, TemplateConfig> TEMPLATES = {
    {"default", {"default", "RoboCute/rbc-project-default", "v0.0.1"}}// Fallback version if latest not available
};

// Get latest release tag from GitHub API
std::string getLatestReleaseTag(const std::string &repo) {
    QNetworkAccessManager manager;
    QEventLoop loop;

    std::string apiUrl = "https://api.github.com/repos/" + repo + "/releases/latest";
    QNetworkRequest request(QUrl(QString::fromStdString(apiUrl)));
    request.setRawHeader("User-Agent", "rbc-cli/1.0");
    request.setRawHeader("Accept", "application/vnd.github.v3+json");

    QNetworkReply *reply = manager.get(request);

    // Handle SSL errors if any
    QObject::connect(reply, &QNetworkReply::sslErrors, [reply](const QList<QSslError> &errors) {
        std::cerr << "SSL Errors:" << std::endl;
        for (const auto &error : errors) {
            std::cerr << "  " << error.errorString().toStdString() << std::endl;
        }
        // Ignore SSL errors for now (not recommended for production)
        reply->ignoreSslErrors();
    });

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        int httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray errorData = reply->readAll();
        std::cerr << "Error fetching latest release:" << std::endl;
        std::cerr << "  Error: " << reply->errorString().toStdString() << std::endl;
        if (httpStatusCode > 0) {
            std::cerr << "  HTTP Status: " << httpStatusCode << std::endl;
        }
        if (!errorData.isEmpty()) {
            std::cerr << "  Response: " << errorData.toStdString() << std::endl;
        }
        reply->deleteLater();
        return "";
    }

    int httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpStatusCode != 200) {
        QByteArray errorData = reply->readAll();
        std::cerr << "Error: HTTP " << httpStatusCode << std::endl;
        if (!errorData.isEmpty()) {
            std::cerr << "Response: " << errorData.toStdString() << std::endl;
        }
        reply->deleteLater();
        return "";
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    if (data.isEmpty()) {
        std::cerr << "Error: Empty response from GitHub API" << std::endl;
        return "";
    }

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        std::cerr << "Error parsing GitHub API response" << std::endl;
        std::cerr << "Response data: " << data.toStdString() << std::endl;
        return "";
    }

    QJsonObject obj = doc.object();
    if (!obj.contains("tag_name")) {
        std::cerr << "Error: tag_name not found in API response" << std::endl;
        std::cerr << "Available keys: ";
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            std::cerr << it.key().toStdString() << " ";
        }
        std::cerr << std::endl;
        return "";
    }

    return obj["tag_name"].toString().toStdString();
}

// Download file from URL (internal helper)
bool downloadFileInternal(const std::string &url, const std::string &outputPath) {
    QNetworkAccessManager manager;
    QEventLoop loop;

    QUrl requestUrl(QString::fromStdString(url));
    QNetworkRequest request(requestUrl);
    // Use a browser-like User-Agent to avoid GitHub blocking
    request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
    request.setRawHeader("Accept", "*/*");
    request.setRawHeader("Accept-Language", "en-US,en;q=0.9");
    // Enable redirects (Qt follows redirects by default, but we can be explicit)
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = manager.get(request);

    // Handle SSL errors if any
    QObject::connect(reply, &QNetworkReply::sslErrors, [reply](const QList<QSslError> &errors) {
        std::cerr << "\nSSL Errors:" << std::endl;
        for (const auto &error : errors) {
            std::cerr << "  " << error.errorString().toStdString() << std::endl;
        }
        reply->ignoreSslErrors();
    });

    QObject::connect(reply, &QNetworkReply::downloadProgress, [](qint64 bytesReceived, qint64 bytesTotal) {
        if (bytesTotal > 0) {
            int percent = (bytesReceived * 100) / bytesTotal;
            std::cout << "\rDownloading: " << percent << "%" << std::flush;
        }
    });

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        int httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray errorData = reply->readAll();
        std::cerr << "\nError downloading file:" << std::endl;
        std::cerr << "  URL: " << url << std::endl;
        std::cerr << "  Error: " << reply->errorString().toStdString() << std::endl;
        if (httpStatusCode > 0) {
            std::cerr << "  HTTP Status: " << httpStatusCode << std::endl;
        }
        if (!errorData.isEmpty()) {
            std::cerr << "  Response: " << errorData.toStdString() << std::endl;
        }
        reply->deleteLater();
        return false;
    }

    int httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpStatusCode != 200 && httpStatusCode != 0) {
        QByteArray errorData = reply->readAll();
        std::cerr << "\nError: HTTP " << httpStatusCode << std::endl;
        std::cerr << "URL: " << url << std::endl;
        if (!errorData.isEmpty()) {
            std::cerr << "Response: " << errorData.toStdString() << std::endl;
        }
        reply->deleteLater();
        return false;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    if (data.isEmpty()) {
        std::cerr << "\nError: Empty response from server" << std::endl;
        std::cerr << "URL: " << url << std::endl;
        return false;
    }

    QFile file(QString::fromStdString(outputPath));
    if (!file.open(QIODevice::WriteOnly)) {
        std::cerr << "\nError opening file for writing: " << outputPath << std::endl;
        return false;
    }

    file.write(data);
    file.close();

    std::cout << "\nDownload completed!" << std::endl;
    return true;
}

// Download file from URL (with fallback for different URL formats)
bool downloadFile(const std::string &url, const std::string &outputPath) {
    // Try the primary URL first
    if (downloadFileInternal(url, outputPath)) {
        return true;
    }

    // If failed and URL contains "/archive/refs/tags/", try simpler format
    std::string fallbackUrl = url;
    size_t pos = fallbackUrl.find("/archive/refs/tags/");
    if (pos != std::string::npos) {
        fallbackUrl.replace(pos, 20, "/archive/");// Replace "/archive/refs/tags/" with "/archive/"
        std::cerr << "Trying alternative URL format: " << fallbackUrl << std::endl;
        return downloadFileInternal(fallbackUrl, outputPath);
    }

    // If failed and URL contains "/archive/", try with refs/tags format
    pos = fallbackUrl.find("/archive/");
    if (pos != std::string::npos && fallbackUrl.find("/refs/tags/") == std::string::npos) {
        std::string tagPart = fallbackUrl.substr(pos + 9);// Get part after "/archive/"
        fallbackUrl = fallbackUrl.substr(0, pos) + "/archive/refs/tags/" + tagPart;
        std::cerr << "Trying alternative URL format: " << fallbackUrl << std::endl;
        return downloadFileInternal(fallbackUrl, outputPath);
    }

    return false;
}

// Extract ZIP file using Qt private QZipReader API
bool extractZip(const std::string &zipPath, const std::string &extractTo) {
    QString zipFilePath = QString::fromStdString(zipPath);
    QString extractPath = QString::fromStdString(extractTo);

    // Create extraction directory
    QDir dir;
    if (!dir.mkpath(extractPath)) {
        std::cerr << "Error creating extraction directory: " << extractTo << std::endl;
        return false;
    }

    std::cout << "Extracting files..." << std::endl;

    // Use Qt private QZipReader API
    QZipReader zip(zipFilePath);
    if (!zip.exists()) {
        std::cerr << "Error: ZIP file does not exist or cannot open: " << zipPath << std::endl;
        return false;
    }

    if (!zip.extractAll(extractPath)) {
        std::cerr << "Error extracting ZIP file" << std::endl;
        zip.close();
        return false;
    }

    zip.close();

    std::cout << "Extraction completed!" << std::endl;
    return true;
}

// Copy directory recursively using Qt
bool copyDirectory(const QString &srcPath, const QString &dstPath) {
    QDir srcDir(srcPath);
    if (!srcDir.exists()) {
        std::cerr << "Error: Source directory does not exist: " << srcPath.toStdString() << std::endl;
        return false;
    }

    QDir dstDir;
    if (!dstDir.mkpath(dstPath)) {
        std::cerr << "Error: Cannot create destination directory: " << dstPath.toStdString() << std::endl;
        return false;
    }

    // Copy all files and subdirectories
    QStringList entries = srcDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        QString srcFilePath = srcDir.absoluteFilePath(entry);
        QString dstFilePath = QDir(dstPath).absoluteFilePath(entry);

        QFileInfo fileInfo(srcFilePath);
        if (fileInfo.isDir()) {
            // Recursively copy subdirectory
            if (!copyDirectory(srcFilePath, dstFilePath)) {
                return false;
            }
        } else {
            // Copy file
            if (!QFile::copy(srcFilePath, dstFilePath)) {
                std::cerr << "Error: Cannot copy file: " << srcFilePath.toStdString() << std::endl;
                return false;
            }
        }
    }

    return true;
}

// Get cache directory path
QString getCacheDirectory() {
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString cachePath = QDir(appDataPath).absoluteFilePath("rbc");
    return cachePath;
}

// Create project from template
int createProject(const std::string &projectName, const std::string &templateName, const std::string &specifiedVersion = "") {
    // Find template configuration
    auto it = TEMPLATES.find(templateName);
    if (it == TEMPLATES.end()) {
        std::cerr << "Error: Unknown template '" << templateName << "'" << std::endl;
        std::cerr << "Available templates: ";
        for (const auto &t : TEMPLATES) {
            std::cerr << t.first << " ";
        }
        std::cerr << std::endl;
        return 1;
    }

    const TemplateConfig &config = it->second;

    std::cout << "Creating project '" << projectName << "' from template '" << templateName << "'..." << std::endl;

    // Determine version to use
    std::string version;
    if (!specifiedVersion.empty()) {
        // User specified version
        version = specifiedVersion;
        std::cout << "Using specified version: " << version << std::endl;
    } else {
        // Try to fetch latest release if defaultVersion is "latest"
        if (config.defaultVersion == "latest") {
            std::cout << "Fetching latest release..." << std::endl;
            std::string latestVersion = getLatestReleaseTag(config.githubRepo);
            if (!latestVersion.empty()) {
                version = latestVersion;
                std::cout << "Latest version found: " << version << std::endl;
            } else {
                // Fallback: use a default version if latest fetch fails
                // Note: You may need to adjust this default version based on your actual repository
                std::cerr << "Warning: Could not fetch latest release version" << std::endl;
                std::cerr << "The repository '" << config.githubRepo << "' may not exist or have no releases." << std::endl;
                std::cerr << "Please specify a version using --version option (e.g., --version v0.0.1)" << std::endl;
                std::cerr << "Or check if the repository exists at: https://github.com/" << config.githubRepo << std::endl;
                return 1;
            }
        } else {
            // Use default version from config
            version = config.defaultVersion;
            std::cout << "Using default version: " << version << std::endl;
        }
    }

    // Construct download URL - try simpler format first (GitHub accepts both formats)
    // Format: https://github.com/owner/repo/archive/tag.zip
    std::string downloadUrl = "https://github.com/" + config.githubRepo + "/archive/" + version + ".zip";
    std::cout << "Download URL: " << downloadUrl << std::endl;

    // Get cache directory
    QString cacheDir = getCacheDirectory();
    QString cacheZipDir = QDir(cacheDir).absoluteFilePath("templates");
    QString cacheExtractDir = QDir(cacheDir).absoluteFilePath("extracted");

    // Create cache directories
    QDir().mkpath(cacheZipDir);
    QDir().mkpath(cacheExtractDir);

    // Cache key: repo-version
    QString cacheKey = QString::fromStdString(config.githubRepo).replace("/", "_") + "_" + QString::fromStdString(version);
    QString zipPath = QDir(cacheZipDir).absoluteFilePath(cacheKey + ".zip");
    QString extractPath = QDir(cacheExtractDir).absoluteFilePath(cacheKey);

    // Check if already cached
    bool needDownload = !QFile::exists(zipPath);
    bool needExtract = !QDir(extractPath).exists() || QDir(extractPath).entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot).isEmpty();

    // Download if needed
    if (needDownload) {
        std::cout << "Downloading template to cache..." << std::endl;
        if (!downloadFile(downloadUrl, zipPath.toStdString())) {
            return 1;
        }
    } else {
        std::cout << "Using cached template..." << std::endl;
    }

    // Extract if needed
    if (needExtract) {
        std::cout << "Extracting template from cache..." << std::endl;
        if (!extractZip(zipPath.toStdString(), extractPath.toStdString())) {
            if (needDownload) {
                QFile::remove(zipPath);// Clean up if we just downloaded it
            }
            return 1;
        }
    } else {
        std::cout << "Using cached extraction..." << std::endl;
    }

    // Find the extracted directory (usually repo-name-tag-name)
    QDir extractDir(extractPath);
    QStringList entries = extractDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    if (entries.isEmpty()) {
        std::cerr << "Error: No directory found in extracted files" << std::endl;
        return 1;
    }

    // Use the first directory found
    QString sourceDir = extractDir.absoluteFilePath(entries.first());

    // Copy to current directory with project name
    std::string currentDir = fs::current_path().string();
    QString targetDir = QDir(QString::fromStdString(currentDir)).absoluteFilePath(QString::fromStdString(projectName));

    // Remove target directory if it exists
    if (QDir(targetDir).exists()) {
        std::cout << "Removing existing project directory..." << std::endl;
        QDir(targetDir).removeRecursively();
    }

    std::cout << "Copying template to project directory..." << std::endl;
    if (!copyDirectory(sourceDir, targetDir)) {
        return 1;
    }

    std::cout << "\nProject '" << projectName << "' created successfully!" << std::endl;
    std::cout << "Template version: " << version << std::endl;

    return 0;
}

int main(int argc, char **argv) {
    // Initialize Qt application (required for network operations)
    QCoreApplication app(argc, argv);

    argparse::ArgumentParser program("rbc", "1.0");

    // Create subparser for commands
    argparse::ArgumentParser createCommand("create");
    createCommand.add_description("Create a new project from a template");
    createCommand.add_argument("project_name")
        .help("Name of the project to create");
    createCommand.add_argument("-t", "--template")
        .default_value(std::string("default"))
        .help("Template to use (default: default)");
    createCommand.add_argument("-v", "--version")
        .help("Specify template version (e.g., v0.0.1). If not specified, will try to fetch latest release");

    program.add_subparser(createCommand);

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception &err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return 1;
    }

    // Handle create command
    if (program.is_subcommand_used("create")) {
        auto projectName = createCommand.get<std::string>("project_name");
        auto templateName = createCommand.get<std::string>("--template");
        std::string version = "";
        try {
            version = createCommand.get<std::string>("--version");
        } catch (...) {
            // Version not specified, use default behavior
        }

        return createProject(projectName, templateName, version);
    }

    // No command specified, show help
    std::cerr << program;
    return 1;
}
