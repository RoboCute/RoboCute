#pragma once

#include <QWidget>
#include <QFormLayout>
#include <QLabel>
#include <QGroupBox>
#include "RBCEditorRuntime/runtime/SceneSync.h"

namespace rbc {

/**
 * Details Panel Widget
 * 
 * Displays properties of the selected entity, including:
 * - Transform component (position, rotation, scale)
 * - Render component (mesh ID, materials)
 * - Resource metadata (path, state)
 */
class DetailsPanel : public QWidget {
    Q_OBJECT

public:
    explicit DetailsPanel(QWidget *parent = nullptr);
    ~DetailsPanel() override;

    // Show entity details
    void showEntity(const SceneEntity *entity, const EditorResourceMetadata *resource);

    // Clear the panel
    void clear();
    
    // Highlight the panel to indicate selection
    void highlight(bool highlight);

private:
    void setupUI();
    void updateTransformSection(const Transform &transform);
    void updateRenderSection(const RenderComponent &render, const EditorResourceMetadata *resource);

    QVBoxLayout *mainLayout_{};

    // Transform section
    QGroupBox *transformGroup_{};
    QFormLayout *transformLayout_{};
    QLabel *positionLabel_{};
    QLabel *rotationLabel_{};
    QLabel *scaleLabel_{};

    // Render section
    QGroupBox *renderGroup_{};
    QFormLayout *renderLayout_{};
    QLabel *meshIdLabel_{};
    QLabel *meshPathLabel_{};
    QLabel *materialsLabel_{};

    // Info label when nothing selected
    QLabel *infoLabel_;
    
    // Entity ID display
    QLabel *entityIdLabel_;
    QGroupBox *entityInfoGroup_;
    
    // Current entity ID for highlighting
    int currentEntityId_;
    
    // Highlight state
    bool isHighlighted_;
};

}// namespace rbc
