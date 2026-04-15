#ifndef NDBSCHEMA_H
#define NDBSCHEMA_H

#include <QXmlStreamReader>

// Current schema version written to every new .ndb file.
// Increment when a breaking format change is made.
static constexpr int kNdbSchemaVersion = 1;

namespace NdbSchema
{

inline bool ReadSchemaVersion(const QXmlStreamAttributes &attributes, int *schemaVersion)
{
	if(schemaVersion != nullptr)
		*schemaVersion = 0; // legacy v0 format when the attribute is absent

	const auto verAttr = attributes.value(QStringLiteral("schemaVersion"));
	if(verAttr.isEmpty())
		return true;

	bool ok = false;
	const int version = verAttr.toInt(&ok);
	if(!ok)
		return false;

	if(schemaVersion != nullptr)
		*schemaVersion = version;

	return version <= kNdbSchemaVersion;
}

} // namespace NdbSchema

#endif // NDBSCHEMA_H
