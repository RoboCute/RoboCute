#include "RBCEditorRuntime/components/DetailsPanel.h"
#include "RBCEditorRuntime/runtime/HttpClient.h"
#include "RBCEditorRuntime/runtime/SceneSyncManager.h"
#include "RBCEditorRuntime/runtime/EditorContext.h"
#include <QVBoxLayout>
#include <QScrollArea>
#include <QJsonObject>
#include <QJsonArray>

namespace rbc {

DetailsPanel::DetailsPanel(HttpClient *httpClient, EditorContext *context, QWidget *parent)
    : QWidget(parent),
      httpClient_(httpClient),
      context_(context),
      currentEntityId_(-1),
      isHighlighted_(false),
      updatingFromServer_(false) {
    setupUI();
}

DetailsPanel::~DetailsPanel() = default;

void DetailsPanel::setupUI() {
    mainLayout_ = new QVBoxLayout(this);
    mainLayout_->setContentsMargins(5, 5, 5, 5);
    mainLayout_->setSpacing(10);

    // Info label (shown when nothing selected)
    infoLabel_ = new QLabel("Select an entity to view its properties", this);
    infoLabel_->setAlignment(Qt::AlignCenter);
    // Use theme color #666666 (matching Viewport placeholder) for subtle text
    infoLabel_->setStyleSheet("QLabel { color: #666666; font-style: italic; }");
    mainLayout_->addWidget(infoLabel_);

    // Entity Info section (Entity ID display)
    entityInfoGroup_ = new QGroupBox("Entity Info", this);
    auto *entityInfoLayout = new QFormLayout(entityInfoGroup_);
    entityInfoLayout->setContentsMargins(8, 8, 8, 8);
    entityInfoLayout->setSpacing(5);
    
    entityIdLabel_ = new QLabel("", this);
    QFont entityIdFont = entityIdLabel_->font();
    entityIdFont.setBold(true);
    entityIdFont.setPointSize(10);
    entityIdLabel_->setFont(entityIdFont);
    entityInfoLayout->addRow("Entity ID:", entityIdLabel_);
    
    entityInfoGroup_->setVisible(false);
    mainLayout_->addWidget(entityInfoGroup_);

    // Transform section
    transformGroup_ = new QGroupBox("Transform Component", this);
    transformLayout_ = new QFormLayout(transformGroup_);
    transformLayout_->setContentsMargins(8, 8, 8, 8);
    transformLayout_->setSpacing(5);

    // Position controls
    positionWidget_ = new QWidget(this);
    positionLayout_ = new QHBoxLayout(positionWidget_);
    positionLayout_->setContentsMargins(0, 0, 0, 0);
    positionLayout_->setSpacing(5);
    
    positionX_ = new QDoubleSpinBox(this);
    positionX_->setRange(-10000.0, 10000.0);
    positionX_->setDecimals(3);
    positionX_->setSingleStep(0.1);
    positionX_->setSuffix(" X");
    
    positionY_ = new QDoubleSpinBox(this);
    positionY_->setRange(-10000.0, 10000.0);
    positionY_->setDecimals(3);
    positionY_->setSingleStep(0.1);
    positionY_->setSuffix(" Y");
    
    positionZ_ = new QDoubleSpinBox(this);
    positionZ_->setRange(-10000.0, 10000.0);
    positionZ_->setDecimals(3);
    positionZ_->setSingleStep(0.1);
    positionZ_->setSuffix(" Z");
    
    positionLayout_->addWidget(positionX_);
    positionLayout_->addWidget(positionY_);
    positionLayout_->addWidget(positionZ_);
    
    connect(positionX_, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, [this](double) {
        if (!updatingFromServer_ && commitButton_ && currentEntityId_ >= 0) {
            commitButton_->setEnabled(true);
            qDebug() << "DetailsPanel: Position X changed, commit button enabled";
        }
    });
    connect(positionY_, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, [this](double) {
        if (!updatingFromServer_ && commitButton_ && currentEntityId_ >= 0) {
            commitButton_->setEnabled(true);
        }
    });
    connect(positionZ_, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, [this](double) {
        if (!updatingFromServer_ && commitButton_ && currentEntityId_ >= 0) {
            commitButton_->setEnabled(true);
        }
    });
    
    transformLayout_->addRow("Position:", positionWidget_);

    // Rotation controls (quaternion)
    rotationWidget_ = new QWidget(this);
    rotationLayout_ = new QHBoxLayout(rotationWidget_);
    rotationLayout_->setContentsMargins(0, 0, 0, 0);
    rotationLayout_->setSpacing(5);
    
    rotationX_ = new QDoubleSpinBox(this);
    rotationX_->setRange(-1.0, 1.0);
    rotationX_->setDecimals(3);
    rotationX_->setSingleStep(0.01);
    rotationX_->setSuffix(" X");
    
    rotationY_ = new QDoubleSpinBox(this);
    rotationY_->setRange(-1.0, 1.0);
    rotationY_->setDecimals(3);
    rotationY_->setSingleStep(0.01);
    rotationY_->setSuffix(" Y");
    
    rotationZ_ = new QDoubleSpinBox(this);
    rotationZ_->setRange(-1.0, 1.0);
    rotationZ_->setDecimals(3);
    rotationZ_->setSingleStep(0.01);
    rotationZ_->setSuffix(" Z");
    
    rotationW_ = new QDoubleSpinBox(this);
    rotationW_->setRange(-1.0, 1.0);
    rotationW_->setDecimals(3);
    rotationW_->setSingleStep(0.01);
    rotationW_->setSuffix(" W");
    
    rotationLayout_->addWidget(rotationX_);
    rotationLayout_->addWidget(rotationY_);
    rotationLayout_->addWidget(rotationZ_);
    rotationLayout_->addWidget(rotationW_);
    
    connect(rotationX_, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, [this](double) {
        if (!updatingFromServer_ && commitButton_ && currentEntityId_ >= 0) {
            commitButton_->setEnabled(true);
        }
    });
    connect(rotationY_, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, [this](double) {
        if (!updatingFromServer_ && commitButton_ && currentEntityId_ >= 0) {
            commitButton_->setEnabled(true);
        }
    });
    connect(rotationZ_, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, [this](double) {
        if (!updatingFromServer_ && commitButton_ && currentEntityId_ >= 0) {
            commitButton_->setEnabled(true);
        }
    });
    connect(rotationW_, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, [this](double) {
        if (!updatingFromServer_ && commitButton_ && currentEntityId_ >= 0) {
            commitButton_->setEnabled(true);
        }
    });
    
    transformLayout_->addRow("Rotation:", rotationWidget_);

    // Scale controls
    scaleWidget_ = new QWidget(this);
    scaleLayout_ = new QHBoxLayout(scaleWidget_);
    scaleLayout_->setContentsMargins(0, 0, 0, 0);
    scaleLayout_->setSpacing(5);
    
    scaleX_ = new QDoubleSpinBox(this);
    scaleX_->setRange(0.001, 1000.0);
    scaleX_->setDecimals(3);
    scaleX_->setSingleStep(0.1);
    scaleX_->setSuffix(" X");
    
    scaleY_ = new QDoubleSpinBox(this);
    scaleY_->setRange(0.001, 1000.0);
    scaleY_->setDecimals(3);
    scaleY_->setSingleStep(0.1);
    scaleY_->setSuffix(" Y");
    
    scaleZ_ = new QDoubleSpinBox(this);
    scaleZ_->setRange(0.001, 1000.0);
    scaleZ_->setDecimals(3);
    scaleZ_->setSingleStep(0.1);
    scaleZ_->setSuffix(" Z");
    
    scaleLayout_->addWidget(scaleX_);
    scaleLayout_->addWidget(scaleY_);
    scaleLayout_->addWidget(scaleZ_);
    
    connect(scaleX_, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, [this](double) {
        if (!updatingFromServer_ && commitButton_ && currentEntityId_ >= 0) {
            commitButton_->setEnabled(true);
        }
    });
    connect(scaleY_, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, [this](double) {
        if (!updatingFromServer_ && commitButton_ && currentEntityId_ >= 0) {
            commitButton_->setEnabled(true);
        }
    });
    connect(scaleZ_, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, [this](double) {
        if (!updatingFromServer_ && commitButton_ && currentEntityId_ >= 0) {
            commitButton_->setEnabled(true);
        }
    });
    
    transformLayout_->addRow("Scale:", scaleWidget_);
    
    // Commit button
    commitButton_ = new QPushButton("Commit Changes", this);
    commitButton_->setEnabled(false);
    commitButton_->setMinimumHeight(30);
    commitButton_->setStyleSheet(
        "QPushButton { "
        "background-color: #007acc; "
        "color: white; "
        "border: none; "
        "border-radius: 4px; "
        "padding: 6px 12px; "
        "font-weight: bold; "
        "}"
        "QPushButton:hover:enabled { "
        "background-color: #0098ff; "
        "}"
        "QPushButton:disabled { "
        "background-color: #3c3c3c; "
        "color: #666666; "
        "}"
    );
    connect(commitButton_, &QPushButton::clicked, this, &DetailsPanel::onCommitClicked);
    transformLayout_->addRow("", commitButton_);

    transformGroup_->setVisible(false);
    mainLayout_->addWidget(transformGroup_);

    // Render section
    renderGroup_ = new QGroupBox("Render Component", this);
    renderLayout_ = new QFormLayout(renderGroup_);
    renderLayout_->setContentsMargins(8, 8, 8, 8);
    renderLayout_->setSpacing(5);

    meshIdLabel_ = new QLabel("", this);
    meshPathLabel_ = new QLabel("", this);
    meshPathLabel_->setWordWrap(true);
    materialsLabel_ = new QLabel("", this);

    renderLayout_->addRow("Mesh ID:", meshIdLabel_);
    renderLayout_->addRow("Mesh Path:", meshPathLabel_);
    renderLayout_->addRow("Material:", materialsLabel_);

    renderGroup_->setVisible(false);
    mainLayout_->addWidget(renderGroup_);

    // Add stretch to push everything to the top
    mainLayout_->addStretch();
}

void DetailsPanel::showEntity(const SceneEntity *entity, const EditorResourceMetadata *resource) {
    if (!entity) {
        clear();
        return;
    }

    // Update current entity ID
    currentEntityId_ = entity->id;
    
    // Reset commit button state when showing a new entity
    if (commitButton_) {
        commitButton_->setEnabled(false);
    }

    // Hide info label, show property groups
    infoLabel_->setVisible(false);
    entityInfoGroup_->setVisible(true);
    transformGroup_->setVisible(true);

    // Update entity ID display
    entityIdLabel_->setText(QString::number(entity->id));

    // Update transform (this will set the spinbox values)
    updateTransformSection(entity->transform);

    // Update render component if present
    if (entity->has_render_component) {
        renderGroup_->setVisible(true);
        updateRenderSection(entity->render_component, resource);
    } else {
        renderGroup_->setVisible(false);
    }
    
    // Apply highlighting if needed
    highlight(isHighlighted_);
}

void DetailsPanel::clear() {
    currentEntityId_ = -1;
    infoLabel_->setVisible(true);
    entityInfoGroup_->setVisible(false);
    transformGroup_->setVisible(false);
    renderGroup_->setVisible(false);
    highlight(false);
    
    // Reset commit button state
    if (commitButton_) {
        commitButton_->setEnabled(false);
    }
}

void DetailsPanel::highlight(bool highlight) {
    isHighlighted_ = highlight;
    
    if (highlight && currentEntityId_ >= 0) {
        // Apply highlight style matching dark theme (main.qss)
        // Use #007acc (accent blue) for border and #094771 (selected background) for highlight
        setStyleSheet(
            "QGroupBox { "
            "border: 2px solid #007acc; "
            "border-radius: 4px; "
            "background-color: #252526; "
            "color: #cccccc; "
            "}"
            "QGroupBox::title { "
            "color: #007acc; "
            "font-weight: bold; "
            "}"
            "QLabel { "
            "color: #cccccc; "
            "}"
        );
        
        // Also highlight entity ID label specifically with accent color
        if (entityIdLabel_) {
            entityIdLabel_->setStyleSheet(
                "QLabel { "
                "color: #ffffff; "
                "font-weight: bold; "
                "background-color: #094771; "
                "padding: 3px 8px; "
                "border-radius: 3px; "
                "border: 1px solid #007acc; "
                "}"
            );
        }
    } else {
        // Reset to default style (inherit from main.qss)
        setStyleSheet("");
        
        if (entityIdLabel_) {
            entityIdLabel_->setStyleSheet("");
        }
    }
}

void DetailsPanel::updateTransformSection(const Transform &transform) {
    updatingFromServer_ = true;
    setTransformValues(transform);
    updatingFromServer_ = false;
}

void DetailsPanel::setTransformValues(const Transform &transform) {
    // Set position values
    positionX_->setValue(transform.position.x);
    positionY_->setValue(transform.position.y);
    positionZ_->setValue(transform.position.z);
    
    // Set rotation values (quaternion)
    rotationX_->setValue(transform.rotation.x);
    rotationY_->setValue(transform.rotation.y);
    rotationZ_->setValue(transform.rotation.z);
    rotationW_->setValue(transform.rotation.w);
    
    // Set scale values
    scaleX_->setValue(transform.scale.x);
    scaleY_->setValue(transform.scale.y);
    scaleZ_->setValue(transform.scale.z);
}

SceneSyncManager *DetailsPanel::getSceneSyncManager() const {
    return context_ ? context_->sceneSyncManager : nullptr;
}

void DetailsPanel::onCommitClicked() {
    qDebug() << "DetailsPanel: Commit button clicked";
    
    if (currentEntityId_ < 0) {
        qWarning() << "DetailsPanel: No entity selected (currentEntityId_ < 0)";
        return;
    }
    
    if (!httpClient_) {
        qWarning() << "DetailsPanel: HttpClient is null";
        return;
    }
    
    SceneSyncManager *sceneSyncManager = getSceneSyncManager();
    if (!sceneSyncManager) {
        qWarning() << "DetailsPanel: SceneSyncManager is null (not connected to server yet?)";
        return;
    }
    
    qDebug() << "DetailsPanel: Sending transform update for entity" << currentEntityId_;
    sendTransformUpdate();
    
    // Note: Don't disable button here - wait for server response
}

void DetailsPanel::sendTransformUpdate() {
    if (!httpClient_ || currentEntityId_ < 0) {
        qWarning() << "DetailsPanel: Cannot send update - missing dependencies";
        return;
    }
    
    SceneSyncManager *sceneSyncManager = getSceneSyncManager();
    if (!sceneSyncManager) {
        qWarning() << "DetailsPanel: SceneSyncManager is null";
        return;
    }
    
    QString editorId = sceneSyncManager->editorId();
    if (editorId.isEmpty()) {
        qWarning() << "DetailsPanel: Editor ID is empty";
        return;
    }
    
    qDebug() << "DetailsPanel: Editor ID:" << editorId;
    
    // Build component data
    QJsonObject componentData;
    
    // Position array
    QJsonArray positionArray;
    positionArray.append(positionX_->value());
    positionArray.append(positionY_->value());
    positionArray.append(positionZ_->value());
    componentData["position"] = positionArray;
    
    // Rotation array (quaternion)
    QJsonArray rotationArray;
    rotationArray.append(rotationX_->value());
    rotationArray.append(rotationY_->value());
    rotationArray.append(rotationZ_->value());
    rotationArray.append(rotationW_->value());
    componentData["rotation"] = rotationArray;
    
    // Scale array
    QJsonArray scaleArray;
    scaleArray.append(scaleX_->value());
    scaleArray.append(scaleY_->value());
    scaleArray.append(scaleZ_->value());
    componentData["scale"] = scaleArray;
    
    qDebug() << "DetailsPanel: Component data - Position:" << positionArray
             << "Rotation:" << rotationArray << "Scale:" << scaleArray;
    
    // Build command params
    QJsonObject params;
    params["entity_id"] = currentEntityId_;
    params["component_type"] = "transform";
    params["component_data"] = componentData;
    
    qDebug() << "DetailsPanel: Sending command - entity_id:" << currentEntityId_
             << "component_type: transform";
    
    // Send command to server
    httpClient_->sendEditorCommand(editorId, "modify_component", params, [this](bool success) {
        qDebug() << "DetailsPanel: Server response - success:" << success;
        if (success) {
            qDebug() << "DetailsPanel: Transform component updated successfully";
            // Disable commit button after successful commit
            if (commitButton_) {
                commitButton_->setEnabled(false);
            }
        } else {
            // Re-enable commit button on failure so user can retry
            if (commitButton_) {
                commitButton_->setEnabled(true);
            }
            qWarning() << "DetailsPanel: Failed to update transform component";
        }
    });
}

void DetailsPanel::updateRenderSection(const RenderComponent &render,
                                       const EditorResourceMetadata *resource) {
    // Mesh ID
    meshIdLabel_->setText(QString::number(render.mesh_id));

    // Mesh path
    if (resource) {
        QString path = QString(resource->path.c_str());
        meshPathLabel_->setText(path);
    } else {
        meshPathLabel_->setText("(Resource not found)");
    }

    // Materials
    if (render.material_ids.empty()) {
        materialsLabel_->setText("Default (OpenPBR)");
    } else {
        QString matStr;
        for (size_t i = 0; i < render.material_ids.size(); ++i) {
            if (i > 0) matStr += ", ";
            matStr += QString::number(render.material_ids[i]);
        }
        materialsLabel_->setText(matStr);
    }
}

}// namespace rbc
