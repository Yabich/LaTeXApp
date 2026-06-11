#pragma once

#ifdef LATEXAPP_HAS_QTPDF

#include <QPointF>
#include <QtPdfWidgets/QPdfView>

class PdfPreviewView final : public QPdfView {
    Q_OBJECT

public:
    explicit PdfPreviewView(QWidget *parent = nullptr);

signals:
    void pdfSyncRequested(int oneBasedPage, QPointF pagePoint);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    bool pointToPagePoint(const QPoint &viewportPoint, int *oneBasedPage, QPointF *pagePoint) const;
    qreal scaleForPage(int page) const;
};

#endif
