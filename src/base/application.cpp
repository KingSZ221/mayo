/****************************************************************************
** Copyright (c) 2020, Fougue Ltd. <http://www.fougue.pro>
** All rights reserved.
** See license at https://github.com/fougue/mayo/blob/master/LICENSE.txt
****************************************************************************/

#include "application.h"

#include <fougtools/occtools/qt_utils.h>

#include <BinXCAFDrivers_DocumentRetrievalDriver.hxx>
#include <BinXCAFDrivers_DocumentStorageDriver.hxx>
#include <CDF_Session.hxx>
#include <STEPCAFControl_Controller.hxx>
#include <XCAFApp_Application.hxx>
#include <XmlXCAFDrivers_DocumentRetrievalDriver.hxx>
#include <XmlXCAFDrivers_DocumentStorageDriver.hxx>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QSettings>
#include <QtCore/QtDebug>

namespace Mayo {

class Document::FormatBinaryRetrievalDriver : public BinXCAFDrivers_DocumentRetrievalDriver {
public:
    opencascade::handle<CDM_Document> CreateDocument() override { return new Document;  }
};

class Document::FormatXmlRetrievalDriver : public XmlXCAFDrivers_DocumentRetrievalDriver {
public:
    opencascade::handle<CDM_Document> CreateDocument() override { return new Document; }
};

ApplicationPtr Application::instance()
{
    static ApplicationPtr appPtr;
    if (appPtr.IsNull()) {
        appPtr = new Application;
        STEPCAFControl_Controller::Init();
        const char strFougueCopyright[] = "Copyright (c) 2020, Fougue Ltd. <http://www.fougue.pro>";
        appPtr->DefineFormat(
                    Document::NameFormatBinary, qUtf8Printable(tr("Binary Mayo Document Format")), "myb",
                    new Document::FormatBinaryRetrievalDriver,
                    new BinXCAFDrivers_DocumentStorageDriver);
        appPtr->DefineFormat(
                    Document::NameFormatXml, qUtf8Printable(tr("XML Mayo Document Format")), "myx",
                    new Document::FormatXmlRetrievalDriver,
                    new XmlXCAFDrivers_DocumentStorageDriver(strFougueCopyright));
    }

    return appPtr;
}

int Application::documentCount() const
{
    return this->NbDocuments();
}

DocumentPtr Application::newDocument(Document::Format docFormat)
{
    const char* docNameFormat = Document::toNameFormat(docFormat);
    Handle_TDocStd_Document stdDoc;
    this->NewDocument(docNameFormat, stdDoc);
    return DocumentPtr::DownCast(stdDoc);
}

DocumentPtr Application::openDocument(const QString& filePath, PCDM_ReaderStatus* ptrReadStatus)
{
    Handle_TDocStd_Document stdDoc;
    const PCDM_ReaderStatus readStatus = this->Open(occ::QtUtils::toOccExtendedString(filePath), stdDoc);
    if (ptrReadStatus)
        *ptrReadStatus = readStatus;

    DocumentPtr doc = DocumentPtr::DownCast(stdDoc);
    this->addDocument(doc);
    return doc;
}

DocumentPtr Application::findDocumentByIndex(int docIndex) const
{
    Handle_TDocStd_Document doc;
    TDocStd_Application::GetDocument(docIndex + 1, doc);
    return !doc.IsNull() ? DocumentPtr::DownCast(doc) : DocumentPtr();
}

DocumentPtr Application::findDocumentByIdentifier(Document::Identifier docIdent) const
{
    auto itFound = m_mapIdentifierDocument.find(docIdent);
    return itFound != m_mapIdentifierDocument.cend() ? itFound->second : DocumentPtr();
}

DocumentPtr Application::findDocumentByLocation(const QFileInfo& loc) const
{
    const QString locAbsoluteFilePath = loc.absoluteFilePath();
    for (const auto& mapPair : m_mapIdentifierDocument) {
        const DocumentPtr& docPtr = mapPair.second;
        if (QFileInfo(docPtr->filePath()).absoluteFilePath() == locAbsoluteFilePath)
            return docPtr;
    }

    return DocumentPtr();
}

int Application::findIndexOfDocument(const DocumentPtr& doc) const
{
    for (DocumentIterator it(this); it.hasNext(); it.next()) {
        if (it.current() == doc)
            return it.currentIndex();
    }

    return -1;
}

void Application::closeDocument(const DocumentPtr& doc)
{
    TDocStd_Application::Close(doc);
}

void Application::setOpenCascadeEnvironment(const QString& settingsFilepath)
{
    const QFileInfo fiSettingsFilepath(settingsFilepath);
    if (!fiSettingsFilepath.exists() || !fiSettingsFilepath.isReadable()) {
        qDebug() << settingsFilepath << "doesn't exist or is not readable";
        return;
    }

    const QSettings occSettings(settingsFilepath, QSettings::IniFormat);
    if (occSettings.status() != QSettings::NoError) {
        qDebug() << settingsFilepath << "could not be loaded by QSettings";
        return;
    }

    const char* arrayOptionName[] = {
        "MMGT_OPT",
        "MMGT_CLEAR",
        "MMGT_REENTRANT",
        "CSF_LANGUAGE",
        "CSF_EXCEPTION_PROMPT"
    };
    const char* arrayPathName[] = {
        "CSF_SHMessage",
        "CSF_MDTVTexturesDirectory",
        "CSF_ShadersDirectory",
        "CSF_XSMessage",
        "CSF_TObjMessage",
        "CSF_StandardDefaults",
        "CSF_PluginDefaults",
        "CSF_XCAFDefaults",
        "CSF_TObjDefaults",
        "CSF_StandardLiteDefaults",
        "CSF_IGESDefaults",
        "CSF_STEPDefaults",
        "CSF_XmlOcafResource",
        "CSF_MIGRATION_TYPES"
    };

    // Process options
    for (const char* varName : arrayOptionName) {
        const QLatin1String qVarName(varName);
        if (!occSettings.contains(qVarName))
            continue;

        const QString strValue = occSettings.value(qVarName).toString();
        qputenv(varName, strValue.toUtf8());
        qDebug().noquote() << QString("%1 = %2").arg(qVarName).arg(strValue);
    }

    // Process paths
    for (const char* varName : arrayPathName) {
        const QLatin1String qVarName(varName);
        if (!occSettings.contains(qVarName))
            continue;

        QString strPath = occSettings.value(qVarName).toString();
        if (QFileInfo(strPath).isRelative())
            strPath = QCoreApplication::applicationDirPath() + QDir::separator() + strPath;

        strPath = QDir::toNativeSeparators(strPath);
        qputenv(varName, strPath.toUtf8());
        qDebug().noquote() << QString("%1 = %2").arg(qVarName).arg(strPath);
    }
}

void Application::NewDocument(
        const TCollection_ExtendedString& /*format*/,
        opencascade::handle<TDocStd_Document>& outDocument)
{
    //std::lock_guard<std::mutex> lock(Internal::mutex_XCAFApplication);
    //XCAFApp_Application::GetApplication()->NewDocument(format, outDocument);

    // TODO: check format == "mayo" if not throw exception
    // Extended from TDocStd_Application::NewDocument() implementation, ensure that in future
    // OpenCascade versions this code is still compatible!
    DocumentPtr newDoc = new Document;
    CDF_Application::Open(newDoc); // Add the document in the session
    this->addDocument(newDoc);
    outDocument = newDoc;
}

void Application::InitDocument(const opencascade::handle<TDocStd_Document>& doc) const
{
    TDocStd_Application::InitDocument(doc);
    XCAFApp_Application::GetApplication()->InitDocument(doc);
}

Application::Application()
    : QObject(nullptr)
{
    qRegisterMetaType<TreeNodeId>("Mayo::TreeNodeId");
    qRegisterMetaType<TreeNodeId>("TreeNodeId");
    qRegisterMetaType<DocumentPtr>("Mayo::DocumentPtr");
    qRegisterMetaType<DocumentPtr>("DocumentPtr");
}

void Application::notifyDocumentAboutToClose(Document::Identifier docIdent)
{
    auto itFound = m_mapIdentifierDocument.find(docIdent);
    if (itFound != m_mapIdentifierDocument.end()) {
        emit this->documentAboutToClose(itFound->second);
        m_mapIdentifierDocument.erase(itFound);
    }
}

void Application::addDocument(const DocumentPtr& doc)
{
    if (!doc.IsNull()) {
        doc->setIdentifier(m_seqDocumentIdentifier.fetch_add(1));
        m_mapIdentifierDocument.insert({ doc->identifier(), doc });
        this->InitDocument(doc);
        doc->initXCaf();

        QObject::connect(
                    doc.get(), &Document::nameChanged,
                    this, [=](const QString& name) { emit this->documentNameChanged(doc, name); });
        QObject::connect(
                    doc.get(), &Document::entityAdded,
                    this, [=](TreeNodeId entityId) { emit this->documentEntityAdded(doc, entityId); });
        QObject::connect(
                    doc.get(), &Document::entityAboutToBeDestroyed,
                    this, [=](TreeNodeId entityId) { emit this->documentEntityAboutToBeDestroyed(doc, entityId); });
//      QObject::connect(
//                  doc, &Document::itemPropertyChanged,
//                  this, &Application::documentItemPropertyChanged);
        emit documentAdded(doc);
    }
}

Application::DocumentIterator::DocumentIterator(const ApplicationPtr&)
    : CDF_DirectoryIterator(CDF_Session::CurrentSession()->Directory())
{
}

Application::DocumentIterator::DocumentIterator(const Application*)
    : CDF_DirectoryIterator(CDF_Session::CurrentSession()->Directory())
{
}

bool Application::DocumentIterator::hasNext() const
{
    return const_cast<DocumentIterator*>(this)->MoreDocument();
}

void Application::DocumentIterator::next()
{
    this->NextDocument();
    ++m_currentIndex;
}

DocumentPtr Application::DocumentIterator::current() const
{
    return DocumentPtr::DownCast(const_cast<DocumentIterator*>(this)->Document());
}

} // namespace Mayo
