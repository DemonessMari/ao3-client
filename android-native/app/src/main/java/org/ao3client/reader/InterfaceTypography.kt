package org.ao3client.reader

import androidx.compose.material3.Typography
import androidx.compose.ui.text.font.FontFamily

internal fun useImportedInterfaceFont(preferences: Preferences): Boolean =
    preferences.useImportedFontInInterface && ReaderFontStore.validId(preferences.fontFamily)

internal fun interfaceTypography(family: FontFamily): Typography {
    val base=Typography()
    return base.copy(
        displayLarge=base.displayLarge.copy(fontFamily=family),
        displayMedium=base.displayMedium.copy(fontFamily=family),
        displaySmall=base.displaySmall.copy(fontFamily=family),
        headlineLarge=base.headlineLarge.copy(fontFamily=family),
        headlineMedium=base.headlineMedium.copy(fontFamily=family),
        headlineSmall=base.headlineSmall.copy(fontFamily=family),
        titleLarge=base.titleLarge.copy(fontFamily=family),
        titleMedium=base.titleMedium.copy(fontFamily=family),
        titleSmall=base.titleSmall.copy(fontFamily=family),
        bodyLarge=base.bodyLarge.copy(fontFamily=family),
        bodyMedium=base.bodyMedium.copy(fontFamily=family),
        bodySmall=base.bodySmall.copy(fontFamily=family),
        labelLarge=base.labelLarge.copy(fontFamily=family),
        labelMedium=base.labelMedium.copy(fontFamily=family),
        labelSmall=base.labelSmall.copy(fontFamily=family)
    )
}
