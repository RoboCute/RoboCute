#include "RBCEditor/components/DetailsPanel.h"
#include <QVBoxLayout>
#include <QScrollArea>

namespace rbc {

DetailsPanel::DetailsPanel(QWidget *parent)
    : QWidget(parent) {
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
    infoLabel_->setStyleSheet("QLabel { color: gray; font-style: italic; }");
    mainLayout_->addWidget(infoLabel_);

    // Transform section
    transformGroup_ = new QGroupBox("Transform Component", this);
    transformLayout_ = new QFormLayout(transformGroup_);
    transformLayout_->setContentsMargins(8, 8, 8, 8);
    transformLayout_->setSpacing(5);

    positionLabel_ = new QLabel("", this);
    rotationLabel_ = new QLabel("", this);
    scaleLabel_ = new QLabel("", this);

    transformLayout_->addRow("Position:", positionLabel_);
    transformLayout_->addRow("Rotation:", rotationLabel_);
    transformLayout_->addRow("Scale:", scaleLabel_);

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

    // Hide info label, show property groups
    infoLabel_->setVisible(false);
    transformGroup_->setVisible(true);

    // Update transform
    updateTransformSection(entity->transform);

    // Update render component if present
    if (entity->has_render_component) {
        renderGroup_->setVisible(true);
        updateRenderSection(entity->render_component, resource);
    } else {
        renderGroup_->setVisible(false);
    }
}

void DetailsPanel::clear() {
    infoLabel_->setVisible(true);
    transformGroup_->setVisible(false);
    renderGroup_->setVisible(false);
}

void DetailsPanel::updateTransformSection(const Transform &transform) {
    // Format position
    QString posStr = QString("[%1, %2, %3]")
                         .arg(transform.position.x, 0, 'f', 3)
                         .arg(transform.position.y, 0, 'f', 3)
                         .arg(transform.position.z, 0, 'f', 3);
    positionLabel_->setText(posStr);

    // Format rotation (quaternion)
    QString rotStr = QString("[%1, %2, %3, %4]")
                         .arg(transform.rotation.x, 0, 'f', 3)
                         .arg(transform.rotation.y, 0, 'f', 3)
                         .arg(transform.rotation.z, 0, 'f', 3)
                         .arg(transform.rotation.w, 0, 'f', 3);
    rotationLabel_->setText(rotStr + " (Quaternion)");

    // Format scale
    QString scaleStr = QString("[%1, %2, %3]")
                           .arg(transform.scale.x, 0, 'f', 3)
                           .arg(transform.scale.y, 0, 'f', 3)
                           .arg(transform.scale.z, 0, 'f', 3);
    scaleLabel_->setText(scaleStr);
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
