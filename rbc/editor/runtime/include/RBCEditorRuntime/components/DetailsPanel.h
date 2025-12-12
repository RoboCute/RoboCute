#pragma once

#include <QWidget>
#include <QFormLayout>
#include <QLabel>
#include <QGroupBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QPushButton>
#include "RBCEditorRuntime/runtime/SceneSync.h"

namespace rbc {
class HttpClient;
class SceneSyncManager;
class EditorContext;

/**
 * Details Panel Widget
 * 
 * Displays and allows editing of properties of the selected entity, including:
 * - Transform component (position, rotation, scale)
 * - Render component (mesh ID, materials)
 * - Resource metadata (path, state)
 */
class DetailsPanel : public QWidget {
    Q_OBJECT

public:
    explicit DetailsPanel(HttpClient *httpClient, EditorContext *context, QWidget *parent = nullptr);
    ~DetailsPanel() override;

    // Show entity details
    void showEntity(const SceneEntity *entity, const EditorResourceMetadata *resource);

    // Clear the panel
    void clear();
    
    // Highlight the panel to indicate selection
    void highlight(bool highlight);

private slots:
    void onCommitClicked();

private:
    void setupUI();
    void updateTransformSection(const Transform &transform);
    void updateRenderSection(const RenderComponent &render, const EditorResourceMetadata *resource);
    void sendTransformUpdate();
    void setTransformValues(const Transform &transform);
    SceneSyncManager *getSceneSyncManager() const;

    HttpClient *httpClient_{};
    EditorContext *context_{};

    QVBoxLayout *mainLayout_{};

    // Transform section
    QGroupBox *transformGroup_{};
    QFormLayout *transformLayout_{};
    
    // Position controls
    QWidget *positionWidget_{};
    QHBoxLayout *positionLayout_{};
    QDoubleSpinBox *positionX_{};
    QDoubleSpinBox *positionY_{};
    QDoubleSpinBox *positionZ_{};
    
    // Rotation controls (quaternion)
    QWidget *rotationWidget_{};
    QHBoxLayout *rotationLayout_{};
    QDoubleSpinBox *rotationX_{};
    QDoubleSpinBox *rotationY_{};
    QDoubleSpinBox *rotationZ_{};
    QDoubleSpinBox *rotationW_{};
    
    // Scale controls
    QWidget *scaleWidget_{};
    QHBoxLayout *scaleLayout_{};
    QDoubleSpinBox *scaleX_{};
    QDoubleSpinBox *scaleY_{};
    QDoubleSpinBox *scaleZ_{};

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
    
    // Flag to prevent recursive updates
    bool updatingFromServer_;
    
    // Commit button
    QPushButton *commitButton_;
};

}// namespace rbc
