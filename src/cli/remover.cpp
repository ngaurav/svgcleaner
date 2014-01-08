/****************************************************************************
**
** SVG Cleaner is batch, tunable, crossplatform SVG cleaning program.
** Copyright (C) 2012-2014 Evgeniy Reizner
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License along
** with this program; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
**
****************************************************************************/

#include "remover.h"

// TODO: remove equal styles in used element and it use
// TODO: remove items which out of viewbox
//       Anonymous_butterfly_and_flowers.svg
// TODO: remove "tspan" without attributes
// TODO: remove "feBlend" element with in or in2 linked to empty object
//       babayasin_air_hammer.svg
// TODO: xlink:href could not contains uri with spaces, remove this elements
// TODO: remove "symbol"
// TODO: remove 'use' elem linked to empty object
// TODO: remove elem from defs if it used only by one use elem

// TODO: do not remove xmlns attr of editor app when removing is disabled
void Remover::cleanSvgElementAttribute()
{
    bool isXlinkUsed = false;
    QList<SvgElement> nodeList = svgElement().childElemList();
    while (!nodeList.isEmpty()) {
        SvgElement currElem = nodeList.takeFirst();
        if (currElem.hasAttribute("xlink:href")) {
            isXlinkUsed = true;
            break;
        }
        if (currElem.hasChildren())
            nodeList << currElem.childElemList();
    }

    StringSet ignoreAttr = Props::styleAttributes;
    ignoreAttr << "xmlns" << "width" << "height" << "viewBox";

    if (Keys.flag(Key::RemoveSvgVersion))
        ignoreAttr << "version";
    if (isXlinkUsed)
        ignoreAttr << "xmlns:xlink";
    foreach (const QString &attrName, svgElement().attributesList()) {
        if (!ignoreAttr.contains(attrName)) {
            svgElement().removeAttribute(attrName);
        }
    }

    if (isXlinkUsed && !svgElement().hasAttribute("xmlns:xlink"))
        svgElement().setAttribute("xmlns:xlink", "http://www.w3.org/1999/xlink");

    // dirty way, but svg cannot be processed by default style cleaning func,
    // because in svg node we cannot remove default values
    if (svgElement().hasAttribute("display")) {
        if (svgElement().attribute("display") == "inline")
            svgElement().removeAttribute("display");
    }
}

void Remover::removeUnusedDefs()
{
    StringSet defsIdList;
    defsIdList << "";
    while (!defsIdList.isEmpty()) {
        defsIdList.clear();

        foreach (const SvgElement &elem, defsElement().childElemList())
            if (elem.tagName() != "clipPath")
                defsIdList << elem.id();

        QList<SvgElement> list = svgElement().childElemList();
        while (!list.isEmpty()) {
            SvgElement elem = list.takeFirst();
            if (elem.hasAttribute("xlink:href"))
                defsIdList.remove(elem.attribute("xlink:href").remove("#"));
            foreach (const QString &attrName, Props::linkableStyleAttributes) {
                if (elem.hasAttribute(attrName)) {
                    QString url = elem.attribute(attrName);
                    defsIdList.remove(url.mid(5, url.size()-6));
                }
            }
            if (elem.hasChildren())
                list << elem.childElemList();
        }

        foreach (const SvgElement &elem, defsElement().childElemList()) {
            if (defsIdList.contains(elem.id()))
                defsElement().removeChild(elem);
        }
    }
}

void Remover::removeUnusedXLinks()
{
    StringSet xlinkSet;
    StringSet idSet;
    QList<SvgElement> list = svgElement().childElemList();
    while (!list.isEmpty()) {
        SvgElement elem = list.takeFirst();
        if (elem.hasAttribute("xlink:href")) {
            if (!elem.attribute("xlink:href").contains("base64"))
                xlinkSet << elem.attribute("xlink:href").remove("#");
        }
        if (elem.hasAttribute("id"))
            idSet << elem.id();

        if (elem.hasChildren())
            list << elem.childElemList();
    }
    foreach (const QString &id, idSet)
        xlinkSet.remove(id);

    list = svgElement().childElemList();
    while (!list.isEmpty()) {
        SvgElement elem = list.takeFirst();
        if (elem.hasAttribute("xlink:href")) {
            if (xlinkSet.contains(elem.attribute("xlink:href").remove("#"))) {
                elem.removeAttribute("xlink:href");
            }
        }
        if (elem.hasChildren())
            list << elem.childElemList();
    }
}

// TODO: detect not only equal, but with diff less than 1%
void Remover::removeDuplicatedDefs()
{
    StringHash xlinkToReplace;
    QList<SvgElement> defsList = defsElement().childElemList();

    // using of structure is faster than actual node accessing
    QList<DefsElemStruct> elemStructList;
    for (int i = 0; i < defsList.count(); ++i) {
        SvgElement elem = defsList.at(i);
        DefsElemStruct es = { elem, elem.tagName(), elem.hasChildren(), elem.attributesMap() };
        elemStructList << es;
    }

    // process gradients
    for (int i = 0; i < elemStructList.count(); ++i) {
        DefsElemStruct des1 = elemStructList.at(i);
        QString id1 = des1.attrMap.value("id");
        if (des1.tagName == "linearGradient" || des1.tagName == "radialGradient") {
            for (int j = i; j < elemStructList.count(); ++j) {
                DefsElemStruct des2 = elemStructList.at(j);
                QString id2 = des2.attrMap.value("id");
                if (des1.tagName == des2.tagName && id1 != id2) {
                    if ((des1.hasChildren && des2.hasChildren)
                        || (!des1.hasChildren && !des2.hasChildren))
                    {
                        bool isRemove = false;
                        if (des1.tagName == "linearGradient")
                            isRemove = Tools::isAttributesEqual(des1.attrMap, des2.attrMap,
                                                            Props::linearGradient);
                        else if (des1.tagName == "radialGradient")
                            isRemove = Tools::isAttributesEqual(des1.attrMap, des2.attrMap,
                                                            Props::radialGradient);
                        if (isRemove) {
                            if (des1.hasChildren && des2.hasChildren)
                                isRemove = Tools::isGradientsEqual(des1.elem, des2.elem);

                            if (isRemove) {
                                if (xlinkToReplace.values().contains(id2)) {
                                    for (int k = 0; k < xlinkToReplace.keys().count(); ++k) {
                                        if (xlinkToReplace.value(xlinkToReplace.keys().at(k)) == id2)
                                            xlinkToReplace.insert(xlinkToReplace.keys().at(k), id1);
                                    }
                                }
                                xlinkToReplace.insert(id2, id1);
                                defsElement().removeChild(des2.elem);
                                elemStructList.removeAt(j);
                                j--;
                            }
                        }
                    }
                }
            }
        }
    }

    // process feGaussianBlur filter
    for (int i = 0; i < elemStructList.count(); ++i) {
        DefsElemStruct des1 = elemStructList.at(i);
        QString id1 = des1.attrMap.value("id");
        if (des1.tagName == "filter") {
            for (int j = i; j < elemStructList.count(); ++j) {
                DefsElemStruct des2 = elemStructList.at(j);
                QString id2 = des2.attrMap.value("id");
                if (des1.tagName == des2.tagName && id1 != id2
                    && des1.elem.childElementCount() == 1 && des2.elem.childElementCount() == 1)
                {
                    if (Tools::isAttributesEqual(des1.attrMap, des2.attrMap, Props::filter)) {
                        SvgElement child1 = des1.elem.firstChild();
                        SvgElement child2 = des2.elem.firstChild();
                        if (   child1.tagName() == "feGaussianBlur"
                            && child2.tagName() == "feGaussianBlur") {
                            if (child1.attribute("stdDeviation")
                                    == child2.attribute("stdDeviation")) {
                                xlinkToReplace.insert(id2, id1);
                                defsElement().removeChild(des2.elem);
                                elemStructList.removeAt(j);
                                j--;
                            }
                        }
                    }
                }
            }
        }
    }

    updateXLinks(xlinkToReplace);
}

void Remover::removeUnreferencedIds()
{
    // find
    StringSet m_allIdList;
    StringSet m_allLinkList;

    QStringList xlinkAttrList;
    xlinkAttrList << "xlink:href" << "inkscape:path-effect" << "inkscape:perspectiveID";

    QStringList urlAttrList = Props::linkableStyleAttributes;

    QList<SvgElement> list = svgElement().childElemList();
    while (!list.isEmpty()) {
        SvgElement elem = list.takeFirst();
        QStringList attrList = elem.attributesList();

        // collect all id's
        if (attrList.contains("id"))
            m_allIdList << elem.id();

        for (int i = 0; i < xlinkAttrList.count(); ++i) {
            if (attrList.contains(xlinkAttrList.at(i)))
                m_allLinkList << elem.attribute(xlinkAttrList.at(i)).remove("#");
        }

        for (int i = 0; i < urlAttrList.count(); ++i) {
            QString attr = urlAttrList.at(i);
            if (attrList.contains(attr)) {
                QString attrValue = elem.attribute(attr);
                if (attrValue.contains("url"))
                    attrValue = attrValue.mid(5, attrValue.size()-6);
                    m_allLinkList << attrValue;
            }
        }

        if (elem.hasChildren())
            list << elem.childElemList();
    }

    // remove all linked ids
    foreach (const QString &text, m_allLinkList)
        m_allIdList.remove(text);

    if (Keys.flag(Key::KeepNamedIds)) {
        // skip id's whithout digits
        foreach (const QString &text, m_allIdList) {
            if (!text.contains(QRegExp("\\d")))
                m_allIdList.remove(text);
        }
    }

    // remove
    list = svgElement().childElemList();
    while (!list.isEmpty()) {
        SvgElement currElem = list.takeFirst();

        if (m_allIdList.contains(currElem.id()))
            currElem.removeAttribute("id");

        if (m_allIdList.contains(currElem.attribute("clip-path")))
            currElem.removeAttribute("id");

        if (currElem.hasChildren())
            list << currElem.childElemList();
    }
}

// TODO: should be in 'while', while no elem is removed
// TODO: add sketch:* elem removing
void Remover::removeElements()
{
    // have to use XMLNode insted of SvgElement, because after converting to element
    // detecting and removing of "comment" or "processing instruction" is impossible
    QList<XMLNode *> nodeList = Tools::childNodeList(document());
    while (!nodeList.isEmpty()) {
        XMLNode *currNode = nodeList.takeFirst();
        SvgElement currElem = SvgElement(currNode->ToElement());

        bool removeThisNode = false;
        if (!currElem.isNull()) {
            QString currTag = currElem.tagName();
            if (currElem.isContainer() && !currElem.hasChildren() && currTag != "glyph"
                && Keys.flag(Key::RemoveEmptyContainers))
                removeThisNode = true;
            else if (currTag == "metadata" && Keys.flag(Key::RemoveMetadata))
                removeThisNode = true;
            else if (currTag.contains("sodipodi") && Keys.flag(Key::RemoveSodipodiElements))
                removeThisNode = true;
            else if (currTag.contains("inkscape") && Keys.flag(Key::RemoveInkscapeElements)
                     && currTag != "inkscape:path-effect")
                removeThisNode = true;
            else if (currTag.startsWith("a:") && Keys.flag(Key::RemoveAdobeElements))
                removeThisNode = true;
            else if (currTag.startsWith("v:") && Keys.flag(Key::RemoveMSVisioElements))
                removeThisNode = true;
            else if (currTag.startsWith("c:") && Keys.flag(Key::RemoveCorelDrawElements))
                removeThisNode = true;
            else if (currTag == "foreignObject")
                removeThisNode = true;
            else if (currTag == "title")
                removeThisNode = true;
            else if (currTag == "desc")
                removeThisNode = true;
            else if (currTag == "script")
                removeThisNode = true;
            else if (currTag == "defs" && !currElem.hasChildren())
                removeThisNode = true;
            else if (currTag == "linearGradient" || currTag == "radialGradient") {
                if (!currElem.hasChildren() && !currElem.hasAttribute("xlink:href"))
                    removeThisNode = true;
            } else if (currTag == "image" && !currElem.attribute("xlink:href").contains("base64"))
                removeThisNode = true;
            else if (currElem.isReferenced() && !currElem.hasAttribute("id")
                     && currElem.parentElement().tagName() == "defs")
                removeThisNode = true;
            else if (!Props::svgElementList.contains(currElem.tagName())
                     && !currNode->ToText() != 0
                     && Keys.flag(Key::RemoveNonSvgElements)) {
                removeThisNode = true;
            } else if (isInvisibleElementsExist(currElem) && Keys.flag(Key::RemoveInvisibleElements))
                removeThisNode = true;
        }

        if (currNode->ToComment() != 0 && Keys.flag(Key::RemoveComments))
            removeThisNode = true;
        else if (currNode->ToDeclaration() != 0 && Keys.flag(Key::RemoveProcInstruction))
            removeThisNode = true;
        else if (QString(currNode->Value()).contains(QRegExp("(!|^)DOCTYPE|(!|^)ENTITY|\\]\\>"))
                 && Keys.flag(Key::RemoveProlog))
            removeThisNode = true;

        if (removeThisNode)
            currNode->Parent()->DeleteChild(currNode);

        if (!currNode->NoChildren())
            nodeList << Tools::childNodeList(currNode);
    }

    // ungroup "a" element
    QList<SvgElement> elemList = svgElement().childElemList();
    while (!elemList.isEmpty()) {
        SvgElement currElem = elemList.takeFirst();
        if (currElem.tagName() == "a") {
            if ((currElem.hasAttribute("id") && currElem.attributesCount() == 1)
                || currElem.attributesCount() == 0)
            {
                foreach (const SvgElement &childElem, currElem.childElemList())
                    currElem.parentElement().insertBefore(childElem, currElem);
                currElem.parentElement().removeChild(currElem);
                currElem.clear();
            }
        }
        if (!currElem.isNull())
            if (currElem.hasChildren())
                elemList << currElem.childElemList();
    }

    // distributions-pentubuntu.svg
    // FIXME: switch style attr have to be cleaned before it, and other attr have to be removed
    qreal stdDevLimit = Keys.doubleNumber(Key::RemoveTinyGaussianBlur);
    elemList = svgElement().childElemList();
    while (!elemList.isEmpty()) {
        SvgElement currElem = elemList.takeFirst();
//        if (currElem.tagName() == "switch" && !currElem.hasImportantAttrs()) {
//            SvgElement parent = currElem.parentNode();
//            foreach (const SvgElement &childElem, currElem.childElemList())
//                parent.insertBefore(childElem, currElem);
//            parent.removeChild(currElem);
//        }
        if (currElem.tagName() == "feGaussianBlur") {
            // remove "feGaussianBlur" element with "stdDeviation" value
            // lower than "--std-deviation-limit"
            // FIXME: check for stdDeviation with transform of all linked element applied
            if (stdDevLimit != 0.0) {
                if (currElem.parentElement().childElementCount() == 1) {
                    // 'stdDeviation' can contains not only one value
                    // we process when it contains only one value
                    QString stdDev = currElem.attribute("stdDeviation");
                    if (!stdDev.contains(",") && !stdDev.contains(" ")) {
                        bool ok = true;
                        if (stdDev.toDouble(&ok) <= stdDevLimit) {
                            Q_ASSERT(ok == true);
                            defsElement().removeChild(currElem.parentElement());
                        }
                    }
                }
            }
        }
        if (currElem.hasChildren())
            elemList << currElem.childElemList();
    }
}

bool Remover::isInvisibleElementsExist(const SvgElement &elem)
{
    QString tagName = elem.tagName();
    //remove elements "rect", "pattern" and "image" with height or width <= 0
    if (tagName == "rect" || tagName == "pattern" || tagName == "image") {
        if (elem.hasAttribute("width") && elem.hasAttribute("height")) {
            QRectF rect = Tools::viewBoxRect(svgElement());
            bool ok = false;
            qreal width  = Tools::convertUnitsToPx(elem.attribute("width"), rect.width()).toDouble(&ok);
            Q_ASSERT(ok == true);
            qreal height = Tools::convertUnitsToPx(elem.attribute("height"), rect.height()).toDouble(&ok);
            Q_ASSERT(ok == true);
            if (width <= 0 || height <= 0)
                return true;
        }
    }

    // TODO: ungroup flowPara with only "id" attr
    // ryanlerch_OCAL_Introduction.svg
//    if (tagName == "flowPara") {
//        if (!elem.hasText() && !elem.hasChildren())
//            return true;
//    }

    // remove elements with opacity="0"
    if (elem.hasAttribute("opacity")) {
        if (elem.doubleAttribute("opacity") <= 0) {
            return true;
        }
    }

    // remove elements with "display=none"
    if (elem.hasAttribute("display")) {
        if (elem.attribute("display") == "none")
            return true;
    }

    if (elem.hasAttribute("visibility")) {
        if (elem.attribute("visibility") == "hidden")
            return true;
    }

    // remove "path" elements with empty "d" attr
    if (tagName == "path")
        if (elem.attribute("d").isEmpty())
            return true;

    // A negative value is an error. A value of zero disables rendering of this element.
    if (tagName == "use") {
        if (elem.hasAttribute("width"))
            if (elem.doubleAttribute("width") == 0)
                return true;
        if (elem.hasAttribute("height"))
            if (elem.doubleAttribute("height") == 0)
                return true;
    }

    // remove "polygon", "polyline" elements with empty "points" attr
    if (tagName == "polygon" || tagName == "polyline")
        if (elem.attribute("points").isEmpty())
            return true;

    // remove "circle" elements with "r" <= 0
    if (tagName == "circle")
        if (elem.attribute("r").toDouble() <= 0)
            return true;

    // remove "ellipse" elements with "rx|ry" <= 0
    if (tagName == "ellipse")
        if (   elem.attribute("rx").toFloat() <= 0
            || elem.attribute("ry").toFloat() <= 0)
            return true;

    // remove "switch" with no attributes or with only "id" attribute
    if (tagName == "switch" && !elem.hasChildren()) {
        if (elem.attributesCount() == 0)
            return true;
        else if (elem.hasAttribute("id") && elem.attributesCount() == 1)
            return true;
    }

    return false;
}

// TODO: process '*-rendering' attribute
void Remover::removeAttributes()
{
    QList<SvgElement> list = Tools::childElemList(document());
    while (!list.isEmpty()) {
        SvgElement elem = list.takeFirst();
        QStringList attrList = elem.attributesList();
        if (!attrList.isEmpty()) {
            // sodipodi:type="inkscape:offset" supported only by inkscape,
            // and its creates problems in other renders
            if (Keys.flag(Key::RemoveInkscapeAttributes))
                cleanAttribute(elem, "inkscape", attrList);

            if (Keys.flag(Key::RemoveSodipodiAttributes))
                cleanAttribute(elem, "sodipodi", attrList);

            if (Keys.flag(Key::RemoveAdobeAttributes)) {
                cleanAttribute(elem, "i:", attrList);
                cleanAttribute(elem, "a:", attrList);
            }

            if (Keys.flag(Key::RemoveMSVisioAttributes))
                cleanAttribute(elem, "v:", attrList);

            if (Keys.flag(Key::RemoveCorelDrawAttributes))
                cleanAttribute(elem, "c:", attrList);

            if (Keys.flag(Key::RemoveDefaultAttributes)) {
                if (elem.attribute("spreadMethod") == "pad")
                    elem.removeAttribute("spreadMethod");
                if (elem.tagName() == "clipPath") {
                    if (elem.attribute("clipPathUnits") == "userSpaceOnUse")
                        elem.removeAttribute("clipPathUnits");
                }
            }
            if (!elem.isText() && Keys.flag(Key::RemoveNotAppliedAttributes))
                elem.removeAttribute("text-align");
            if (attrList.contains("desc") && Keys.flag(Key::RemoveNotAppliedAttributes))
                elem.removeAttribute("desc");

            if (elem.tagName() == "stop") {
                if (elem.doubleAttribute("offset") < 0.0001)
                    elem.removeAttribute("offset");
            }

            // TODO: 'display' attr remove

            // remove empty attributes
            for (const XMLAttribute *child = elem.xmlElement()->FirstAttribute(); child;
                 child = child->Next()) {
                if (child->Value() == 0)
                    elem.xmlElement()->DeleteAttribute(child->Name());
            }
        }

        if (elem.hasChildren())
            list << elem.childElemList();
    }


    // remove xml:space when no child has multispace text
    list = Tools::childElemList(document());
    while (!list.isEmpty()) {
        SvgElement elem = list.takeFirst();

        if (elem.hasAttribute("xml:space")) {
            bool canRemove = true;
            QList<XMLNode *> list2 = Tools::childNodeList(elem.xmlElement());
            while (!list2.isEmpty()) {
                XMLNode *elem2 = list2.takeFirst();
                QString tagName = QLatin1String(elem2->Value());
                if (tagName == "tspan" || tagName == "flowPara") {
                    if (elem2->FirstChild() != 0) {
                        if (elem2->FirstChild()->ToText() != 0) {
                            if (QString(elem2->FirstChild()->ToText()->Value()).contains("  ")) {
                                canRemove = false;
                                break;
                            }
                        }
                    }
                }
                if (!elem2->NoChildren())
                    list2 << Tools::childNodeList(elem2);
            }
            if (canRemove)
                elem.removeAttribute("xml:space");
        }

        if (elem.hasChildren())
            list << elem.childElemList();
    }

    removeNonElementAttributes();
}

void Remover::cleanAttribute(SvgElement &elem, const QString &startWith, QStringList &attrList)
{
    foreach (const QString &attrName, attrList) {
        if (attrName.startsWith(startWith)) {
            elem.removeAttribute(attrName);
            attrList.removeOne(attrName);
        }
    }
}

void Remover::removeNonElementAttributes()
{
    StringSet circle;
    circle << "transform" << "cx" << "cy" << "r" << "id";
    circle.unite(Props::styleAttributes);

    StringSet ellipse;
    ellipse << "transform" << "cx" << "cy" << "rx" << "ry" << "id";
    ellipse.unite(Props::styleAttributes);

    QList<SvgElement> elemList = svgElement().childElemList();
    while (!elemList.isEmpty()) {
        SvgElement elem = elemList.takeFirst();
        QString tagName = elem.tagName();
        if (tagName == "circle" || tagName == "ellipse") {
            foreach (const QString &attrName, elem.attributesList()) {
                if (!circle.contains(attrName) && tagName == "circle")
                    elem.removeAttribute(attrName);
                else if (!ellipse.contains(attrName) && tagName == "ellipse")
                    elem.removeAttribute(attrName);
            }
        }
        if (elem.hasChildren())
            elemList << elem.childElemList();
    }
}

void Remover::processStyleAttributes(SvgElement elem)
{
    if (elem.isNull())
        elem = svgElement();

    styleHashList << elem.styleMap();
    parentHash.unite(styleHashList.last());
    parentAttrs = parentHash.keys().toSet();

    QList<SvgElement> list = elem.childElemList();
    while (!list.isEmpty()) {
        SvgElement currElem = list.takeFirst();
        StringMap hash = currElem.styleMap();
        cleanStyle(currElem, hash);
        foreach (const QString &attrName, currElem.styleAttributesList()) {
            if (hash.contains(attrName))
                currElem.setAttribute(attrName, hash.value(attrName));
            else
                currElem.removeAttribute(attrName);
            hash.remove(attrName);
        }
        foreach (const QString &attrName, hash.keys())
            currElem.setAttribute(attrName, hash.value(attrName));
        if (currElem.hasChildren())
            processStyleAttributes(currElem);
    }

    styleHashList.removeLast();
    parentHash.clear();
    foreach (const StringMap &hash, styleHashList)
        parentHash.unite(hash);
}

// removes default value, only if parent style did't contain same attribute
// needed for all inherited attributes
void Remover::cleanStyle(const SvgElement &elem, StringMap &hash)
{
    if (!elem.isText() && Keys.flag(Key::RemoveNotAppliedAttributes)) {
        // remove all text based values from non text elements
        foreach (const QString &attr, hash.keys()) {
            if (attr.contains("font") || attr.contains("text"))
                hash.remove(attr);
        }
        hash.remove("writing-mode");
    }

    // remove style props which already defined in parent style
    foreach (const QString &attr, parentHash.keys()) {
        if (attr != "opacity" && !elem.isUsed()) {
            if (hash.contains(attr))
                if (hash.value(attr) == parentHash.value(attr))
                    hash.remove(attr);
        }
    }

    if (hash.contains("stroke-width"))
        hash.insert("stroke-width", Tools::convertUnitsToPx(hash.value("stroke-width")));
    if (hash.contains("stroke-dashoffset"))
        hash.insert("stroke-dashoffset",
                     Tools::convertUnitsToPx(hash.value("stroke-dashoffset")));

    // remove all fill properties if fill is off
    if (parentAttrs.contains("fill") && parentHash.value("fill") == "none"
            && Keys.flag(Key::RemoveFillProps)
            && (hash.value("fill") == "none" || hash.value("fill-opacity") == "0"))
    {
            foreach (const QString &attr, Props::fillList)
                hash.remove(attr);
    } else {
        if (Keys.flag(Key::ConvertColorToRRGGBB) || Keys.flag(Key::ConvertRRGGBBToRGB)) {
            QString fill = hash.value("fill");
            if (!fill.isEmpty() && fill != "none" && !fill.startsWith("url"))
                hash.insert("fill", Tools::trimColor(fill));
        }
    }

    // remove all stroke properties if stroke is off
    if (  (hash.value("stroke") == "none" || hash.value("stroke-opacity") == "0"
        || hash.value("stroke-width") == "0") && Keys.flag(Key::RemoveStrokeProps)) {
        foreach (const QString &attr, Props::strokeList)
            hash.remove(attr);
        if (parentHash.value("stroke") != "none")
            hash.insert("stroke", "none");
    } else {
        QString stroke = hash.value("stroke");
        if (!stroke.isEmpty()) {
            if (stroke != "none" && !stroke.startsWith("url"))
                hash.insert("stroke", Tools::trimColor(hash.value("stroke")));
        }
        // trim array
        if (hash.contains("stroke-dasharray"))
            hash.insert("stroke-dasharray", QString(hash.value("stroke-dasharray"))
                                             .replace(", ", ","));
    }

    if (Keys.flag(Key::RemoveNotAppliedAttributes))
        hash.remove("pointer-events");

    // remove clip-rule if no clip-path
    if (hash.contains("clip-rule") && !hash.contains("clip-path"))
        hash.remove("clip-rule");

    // 'enable-background' is only applicable to container elements
    if (!elem.isContainer())
        hash.remove("enable-background");

    if (elem.tagName() != "svg" && elem.tagName() != "pattern" && elem.tagName() != "marker")
        hash.remove("overflow");

    if (Keys.flag(Key::RemoveInkscapeAttributes))
        hash.remove("-inkscape-font-specification");

    // convert units
    static QStringList numericStyleList
            = QStringList() << "fill-opacity" << "opacity" << "stop-opacity" << "stroke-miterlimit"
                            << "stroke-opacity" << "stroke-width" << "font-size" << "kerning"
                            << "letter-spacing" << "word-spacing" << "baseline-shift";
    foreach (const QString &key, numericStyleList) {
        QString value = hash.value(key);
        if (!value.isEmpty()) {
            QString lengthType = value.right(2);
            if (Props::lengthTypes.contains(lengthType) && !value.contains("url")) {
                hash.insert(key, Tools::convertUnitsToPx(hash.value(key)));
                value = hash.value(key);
            }
            bool ok = false;
            qreal num = value.toDouble(&ok);
            if (ok) {
                QString number = Tools::roundNumber(num, Tools::ATTRIBUTE);
                hash.insert(key, number);
            }
        }
    }

    // remove default values
    QList<QString> keyList = hash.keys();
    foreach (const QString &attrName, keyList)
        removeDefaultValue(hash, attrName);

    // trim colors
    if (Keys.flag(Key::ConvertColorToRRGGBB)) {
        foreach (const QString &attrName, QStringList() << "color" << "stop-color" << "flood-color") {
            if (hash.contains(attrName))
                hash.insert(attrName, Tools::trimColor(hash.value(attrName)));
        }
    }
}

void Remover::removeDefaultValue(StringMap &hash, const QString &name)
{
    if (parentAttrs.contains(name))
        return;

    if (name == "fill" || name == "stop-color") {
        static QStringList defValues = QStringList() << "#000" << "#000000" << "black";
        if (defValues.contains(hash.value(name)))
            hash.remove(name);
    } else if (Props::defaultStyleValues.contains(name)) {
        const QVariant value = Props::defaultStyleValues.value(name);
        if (value.type() == QVariant::String) {
            if (value == hash.value(name))
                hash.remove(name);
        } else if (!hash.value(name).isEmpty()) {
            if (Tools::strToDouble(hash.value(name)) == value.toDouble())
                hash.remove(name);
        }
    }
}

void Remover::removeGroups()
{
    QStringList illegalGAttrList;
    illegalGAttrList << "mask" << "clip-path" << "filter";

    bool isAnyGroupRemoved = true;
    while (isAnyGroupRemoved) {
        isAnyGroupRemoved = false;
        QList<SvgElement> list = svgElement().childElemList();
        while (!list.isEmpty()) {
            SvgElement elem = list.takeFirst();
            SvgElement parent = elem.parentElement();
            if (elem.isGroup() && elem.childElementCount() == 0) {
                // remove empty group
                parent.removeChild(elem);
            } else if (elem.isGroup() && elem.parentElement().tagName() != "switch") {
                if (!elem.hasImportantAttrs() && !elem.firstChild().hasAttribute("mask")) {
                    // ungroup group without attributes
                    foreach (SvgElement childElem, elem.childElemList())
                        parent.insertBefore(childElem, elem);
                    parent.removeChild(elem);
                    isAnyGroupRemoved = true;
                } else if (!elem.isUsed() && parent.isGroup()
                           && !elem.hasAttributes(illegalGAttrList)) {
                    SvgElement firstChild = elem.firstChild();
                    if (elem.childElementCount() == 1
                        && firstChild.tagName() != "switch")
                    {
                        // ungroup group with only one child
                        parent.insertBefore(firstChild, elem);
                        megreGroups(elem, firstChild, false);
                        parent.removeChild(elem);
                        isAnyGroupRemoved = true;
                    } else if (parent.childElementCount() == 1
                               && parent.tagName() != "svg"
                               && !parent.hasAttributes(illegalGAttrList))
                    {
                        megreGroups(elem, parent, true);
                        foreach (SvgElement childElem, elem.childElemList())
                            parent.insertBefore(childElem, elem);
                        parent.removeChild(elem);
                        isAnyGroupRemoved = true;
                    }
                }
            }
            if (elem.hasChildren())
                list << elem.childElemList();
        }
    }
}

void Remover::megreGroups(SvgElement parentElem, SvgElement childElem, bool isParentToChild)
{
    QStringList ignoreAttrList = QStringList() << "id";
    if (!isParentToChild)
        ignoreAttrList << "stroke" << "stroke-width" << "fill";

    foreach (const QString &attrName, parentElem.attributesList()) {
        if (childElem.hasAttribute(attrName) && attrName == "transform") {
            childElem.setTransform(parentElem.attribute(attrName), !isParentToChild);
        } else if (attrName == "opacity") {
            if (parentElem.hasAttribute("opacity") && childElem.hasAttribute("opacity")) {
                qreal newOp = parentElem.doubleAttribute("opacity")
                              * childElem.doubleAttribute("opacity");
                childElem.setAttribute("opacity", Tools::roundNumber(newOp));
            } else {
                childElem.setAttribute(attrName, parentElem.attribute(attrName));
            }
        } else if (!ignoreAttrList.contains(attrName) || !childElem.hasAttribute(attrName)) {
            childElem.setAttribute(attrName, parentElem.attribute(attrName));
        }
    }
}
