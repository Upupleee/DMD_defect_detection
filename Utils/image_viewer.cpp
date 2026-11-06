#include "image_viewer.h"


ImageViewer::ImageViewer(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAutoFillBackground(false);
    setMouseTracking(false);
    setCursor(Qt::OpenHandCursor);
}

bool ImageViewer::loadImage(const QString& fileName)
{
    QImage img(fileName);
    if (img.isNull()) return false;
    setImage(img);
    return true;
}

void ImageViewer::clearImage()
{
    QMutexLocker locker(&m_mutex);
    m_currentImage = QImage();
    m_displayImage = QImage();
    m_scale  = 1.0;
    m_offset = QPointF(0, 0);
    m_autoFitOnNewFrame = true;
    update();
}

QImage ImageViewer::getCurrentImage() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentImage;
}

void ImageViewer::setImage(const QImage& img)
{
    QMutexLocker locker(&m_mutex);

    const bool firstFrame = m_currentImage.isNull();
    const bool sizeChanged = (!firstFrame && m_currentImage.size() != img.size());

    m_currentImage = img; // 隐式共享，避免大拷贝
    rebuildDisplayImage();

    // 首帧 / 分辨率变化 / 自动模式：整幅图适配
    if (firstFrame || sizeChanged || m_autoFitOnNewFrame) {
        m_scale = calcFitScaleLocked();
        m_offset = QPointF(0, 0);
    }

    update();
}

void ImageViewer::resetView()
{
    QMutexLocker locker(&m_mutex);
    m_autoFitOnNewFrame = true;
    m_scale = calcFitScaleLocked();
    m_offset = QPointF(0, 0);
    update();
}

void ImageViewer::rebuildDisplayImage()
{
    // 注意：调用处已持有 m_mutex
    if (m_currentImage.isNull()) return;
    m_displayImage = m_currentImage; // 始终保留原图，不做 scaled
}


double ImageViewer::calcFitScaleLocked() const
{
    if (m_displayImage.isNull() || m_viewSize.isEmpty()) {
        return 1.0;
    }

    const double sx = static_cast<double>(m_viewSize.width()) / static_cast<double>(m_displayImage.width());
    const double sy = static_cast<double>(m_viewSize.height()) / static_cast<double>(m_displayImage.height());
    return qMin(sx, sy);
}

// ── 绘制 ────────────────────────────────────────────────────────────
void ImageViewer::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.fillRect(rect(), QColor(40, 44, 52));

    QMutexLocker locker(&m_mutex);
    if (m_displayImage.isNull()) return;

    // 以控件中心为锚点，叠加用户偏移，再应用缩放
    QPointF center = QPointF(rect().center()) + m_offset;
    QSizeF  scaledSize = QSizeF(m_displayImage.size()) * m_scale;
    QRectF  target(center.x() - scaledSize.width()  / 2.0,
                   center.y() - scaledSize.height() / 2.0,
                   scaledSize.width(),
                   scaledSize.height());

    painter.drawImage(target, m_displayImage);
}

// ── 控件尺寸改变 ─────────────────────────────────────────────────────
void ImageViewer::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    QMutexLocker locker(&m_mutex);
    m_viewSize = event->size();
    rebuildDisplayImage();

    if (m_autoFitOnNewFrame) {
        m_scale = calcFitScaleLocked();
        m_offset = QPointF(0, 0);
    }

    update();
}

// ── 滚轮缩放（以鼠标位置为中心） ─────────────────────────────────────
void ImageViewer::wheelEvent(QWheelEvent* event)
{
    const double zoomStep   = 1.15;
    const double scaleMin   = 0.02;
    const double scaleMax   = 128.0;

    double factor = (event->angleDelta().y() > 0) ? zoomStep : (1.0 / zoomStep);
    double newScale = qBound(scaleMin, m_scale * factor, scaleMax);

    if (qFuzzyCompare(newScale, m_scale)) return;
    m_autoFitOnNewFrame = false; // 用户手动缩放后，不再被新帧重置

    // 以鼠标在控件中的位置为缩放中心，调整偏移使图像"钉住"鼠标
    QPointF mousePos  = event->position();
    QPointF centerPos = QPointF(rect().center());
    QPointF pivot     = mousePos - centerPos - m_offset; // 鼠标相对于图像中心的坐标

    m_offset += pivot * (1.0 - newScale / m_scale);
    m_scale   = newScale;

    update();
    event->accept();
}

// ── 拖拽平移 ─────────────────────────────────────────────────────────
void ImageViewer::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging        = true;
        m_dragStartPos    = event->pos();
        m_dragStartOffset = m_offset;
        setCursor(Qt::ClosedHandCursor);
        event->accept();
    }
}

void ImageViewer::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        m_offset = m_dragStartOffset + QPointF(event->pos() - m_dragStartPos);
        update();
        event->accept();
    }
}

void ImageViewer::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(Qt::OpenHandCursor);
        event->accept();
    }
}

// ── 双击复位 ─────────────────────────────────────────────────────────
void ImageViewer::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        resetView();
        event->accept();
    }
}
