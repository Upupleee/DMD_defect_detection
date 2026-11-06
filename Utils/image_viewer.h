#ifndef IMAGE_VIEWER_H
#define IMAGE_VIEWER_H

#include <QWidget>
#include <QImage>
#include <QMutex>
#include <QResizeEvent>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QPoint>
#include <QPointF>
#include <QSize>
#include <QPainter>
#include <QFileInfo>
#include <QApplication>
#include <QtGlobal>

class ImageViewer : public QWidget
{
    Q_OBJECT
public:
    explicit ImageViewer(QWidget* parent = nullptr);

    bool loadImage(const QString& fileName);
    void clearImage();
    QImage getCurrentImage() const;
    void setImage(const QImage& img);
    void resetView(); // 双击复位：回到整幅自适应显示

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;       // 滚轮缩放
    void mousePressEvent(QMouseEvent* event) override;  // 拖拽开始
    void mouseMoveEvent(QMouseEvent* event) override;   // 拖拽移动
    void mouseReleaseEvent(QMouseEvent* event) override;// 拖拽结束
    void mouseDoubleClickEvent(QMouseEvent* event) override; // 双击复位

private:
    QImage         m_currentImage;
    QImage         m_displayImage;
    mutable QMutex m_mutex;
    QSize          m_viewSize;

    // 缩放与平移状态
    double  m_scale  = 1.0;   // 实际绘制缩放
    QPointF m_offset;         // 图像中心相对控件中心的偏移
    QPoint  m_dragStartPos;
    QPointF m_dragStartOffset;
    bool    m_dragging = false;

    // true: 新帧自动整幅适配；false: 保持用户当前缩放/平移
    bool m_autoFitOnNewFrame = true;

    void rebuildDisplayImage();
    double calcFitScaleLocked() const;
};

#endif // IMAGE_VIEWER_H
        