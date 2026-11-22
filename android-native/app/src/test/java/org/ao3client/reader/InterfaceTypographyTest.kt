package org.ao3client.reader

import androidx.compose.material3.Typography
import androidx.compose.ui.text.font.FontFamily
import org.junit.Assert.*
import org.junit.Test

class InterfaceTypographyTest {
    @Test fun selectedFamilyCoversMenusPreviewMetadataAndEveryTextRole() {
        val base=Typography();val themed=interfaceTypography(FontFamily.Monospace)
        val before=listOf(base.displayLarge,base.displayMedium,base.displaySmall,base.headlineLarge,base.headlineMedium,base.headlineSmall,base.titleLarge,base.titleMedium,base.titleSmall,base.bodyLarge,base.bodyMedium,base.bodySmall,base.labelLarge,base.labelMedium,base.labelSmall)
        val after=listOf(themed.displayLarge,themed.displayMedium,themed.displaySmall,themed.headlineLarge,themed.headlineMedium,themed.headlineSmall,themed.titleLarge,themed.titleMedium,themed.titleSmall,themed.bodyLarge,themed.bodyMedium,themed.bodySmall,themed.labelLarge,themed.labelMedium,themed.labelSmall)
        before.zip(after).forEach { (original,updated) ->
            assertEquals(FontFamily.Monospace,updated.fontFamily)
            assertEquals(original.fontSize,updated.fontSize);assertEquals(original.fontWeight,updated.fontWeight);assertEquals(original.lineHeight,updated.lineHeight)
        }
    }
    @Test fun interfaceChoiceOnlyAppliesToImportedFontsAndCanBeDisabled() {
        val imported=Preferences(fontFamily="font:"+"a".repeat(64))
        assertTrue(useImportedInterfaceFont(imported))
        assertFalse(useImportedInterfaceFont(imported.copy(useImportedFontInInterface=false)))
        assertFalse(useImportedInterfaceFont(Preferences()))
        assertFalse(useImportedInterfaceFont(Preferences(fontFamily="sans-serif")))
        assertFalse(useImportedInterfaceFont(Preferences(fontFamily="font:missing")))
    }
}
