#include "widgets/PdfPreviewView.h"

#ifdef LATEXAPP_HAS_QTPDF

#include <QMouseEvent>
#include <QScrollBar>
#include <QtPdf/QPdfDocument>
#include <QtPdf/QPdfPageNavigator>

PdfPreviewView::PdfPreviewView(QWidget *parent)
    : QPdfView(parent)
{
}

void PdfPreviewView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && (event->modifiers() & Qt::ControlModifier)) {
        int page = 0;
        QPointF pagePoint;
        if (pointToPagePoint(event->pos(), &page, &pagePoint)) {
            emit pdfSyncRequested(page, pagePoint);
            event->accept();
            return;
        }
    }

    QPdfView::mousePressEvent(event);
}

bool PdfPreviewView::pointToPagePoint(const QPoint &viewportPoint, int *oneBasedPage, QPointF *pagePoint) const
{
    const auto *pdfDocument = document();
    if (!pdfDocument || pdfDocument->pageCount() <= 0) {
        return false;
    }

    const auto contentPoint = QPointF(
        viewportPoint.x() + horizontalScrollBar()->value(),
        viewportPoint.y() + verticalScrollBar()->value());

    const auto margins = documentMargins();
    qreal y = margins.top();
    const auto availableWidth = qMax(1, viewport()->width() - margins.left() - margins.right());

    const int firstPage = pageMode() == QPdfView::PageMode::SinglePage && pageNavigator()
        ? qBound(0, pageNavigator()->currentPage(), pdfDocument->pageCount() - 1)
        : 0;
    const int lastPage = pageMode() == QPdfView::PageMode::SinglePage ? firstPage : pdfDocument->pageCount() - 1;

    for (int page = firstPage; page <= lastPage; ++page) {
        const auto pageSize = pdfDocument->pagePointSize(page);
        if (pageSize.isEmpty()) {
            continue;
        }

        const auto scale = scaleForPage(page);
        const auto pagePixelSize = QSizeF(pageSize.width() * scale, pageSize.height() * scale);
        const auto x = margins.left() + qMax<qreal>(0.0, (availableWidth - pagePixelSize.width()) / 2.0);
        const QRectF pageRect(QPointF(x, y), pagePixelSize);

        if (pageRect.contains(contentPoint)) {
            if (oneBasedPage) {
                *oneBasedPage = page + 1;
            }
            if (pagePoint) {
                *pagePoint = QPointF(
                    (contentPoint.x() - pageRect.left()) / scale,
                    (contentPoint.y() - pageRect.top()) / scale);
            }
            return true;
        }

        y += pagePixelSize.height() + pageSpacing();
    }

    return false;
}

qreal PdfPreviewView::scaleForPage(int page) const
{
    const auto *pdfDocument = document();
    if (!pdfDocument) {
        return 1.0;
    }

    const auto pageSize = pdfDocument->pagePointSize(page);
    if (pageSize.isEmpty()) {
        return 1.0;
    }

    const auto margins = documentMargins();
    const auto availableWidth = qMax<qreal>(1.0, viewport()->width() - margins.left() - margins.right());
    const auto availableHeight = qMax<qreal>(1.0, viewport()->height() - margins.top() - margins.bottom());

    switch (zoomMode()) {
    case QPdfView::ZoomMode::FitToWidth:
        return availableWidth / pageSize.width();
    case QPdfView::ZoomMode::FitInView:
        return qMin(availableWidth / pageSize.width(), availableHeight / pageSize.height());
    case QPdfView::ZoomMode::Custom:
        return qMax<qreal>(0.01, logicalDpiY() / 72.0 * zoomFactor());
    }

    return 1.0;
}

#endif
