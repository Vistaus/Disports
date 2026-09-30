.pragma library

// The size of an attachment's preview in the chat (MediaPreview), from the
// dimensions Discord reports: known before the picture loads, so a message
// can keep room for its media from the start (MessageDelegate).

function aspect(media) {
    return media.width > 0 && media.height > 0 ? media.height / media.width : 0.75
}

function width(media, maxWidth, maxHeight, fileHeight) {
    if (media.kind === "file")
        return maxWidth
    return Math.min(maxWidth, media.width > 0 ? media.width : maxWidth, maxHeight / aspect(media))
}

function height(media, maxWidth, maxHeight, fileHeight) {
    if (media.kind === "file")
        return fileHeight
    return width(media, maxWidth, maxHeight, fileHeight) * aspect(media)
}

// All of a message's media, stacked with `spacing` between them.
function totalHeight(list, maxWidth, maxHeight, fileHeight, spacing) {
    let total = 0
    for (let i = 0; i < list.length; ++i)
        total += height(list[i], maxWidth, maxHeight, fileHeight) + (i > 0 ? spacing : 0)
    return total
}
